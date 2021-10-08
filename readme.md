
## Vitis部署FFT至U50教程

### 1. 介绍
文档主要包括以下内容
- [x] Vitis HLS 以及 Vitis部署流程(据说比传统的Vivado方便)
- [x] Vitis HLS中Vitis target flow和Vivado target flow中综合的区别
- [x] Cosim的结果和最后部署至板子上的结果差异。

工程搭建需要：
- Linux
- Vitis、Vitis_HLS以及XRT
- XilinxU50开发板

| Alveo Card |         Platform                 |   XRT    |
| :--------- | :------------------------------: | :------: |
| U200       | xilinx_u200_xdma_201830_2        | 2.6.655  |
| U250       | xilinx_u250_xdma_201830_2        | 2.6.655  |
| U50        | xilinx_u50_gen3x16_xdma_201920_3 | 2.6.655  |

### 2. Vitis HLS 代码编写

创建Vitis HLS工程，最好另建立文件夹
```python
vitis_hls tcl_script.tcl
``` 
FFT是1024点Radix-2的蝶形形运算，代码整体使用dataflow，使得FFT计算可以流水进行。
共分为10个阶段。例化fft_stages十次，以及输入和输出函数（该过程可以优化，但不是该文档的重点）

==需要注意的一点==这里使用pragma设置带宽256，一次可以传输8个float32.（传统Vivado HLS不支持该操作）。传统基于VIVADO HLS，接口需要设置为ap_uint<256>

```C++
#pragma HLS INTERFACE m_axi max_widen_bitwidth=256 max_read_burst_length=16 bundle=RE_IOUT port=OUT_I
```
IN and OUT function这里可以直接对内层进行UNROLL,II=1(OUT_R,OUT_I需要array partition).
``` C++
typedef union{ unsigned ival;float oval;} converter;
void get_in(DTYPE X_R[SIZE], DTYPE X_I[SIZE],DTYPE OUT_R[SIZE], DTYPE OUT_I[SIZE]){
	GIN:for(unsigned short i =0;i< SIZE/KNUM;i++){
#pragma HLS PIPELINE II=1
		ap_uint<256> TMP_R,TMP_I;
		memcpy(&TMP_R,(ap_uint<256>*)X_R+i,KNUM*sizeof(DTYPE));
		memcpy(&TMP_I,(ap_uint<256>*)X_I+i,KNUM*sizeof(DTYPE));
		for(unsigned short k=0;k<KNUM;k++){
#pragma HLS UNROLL
			unsigned short index = i*KNUM+k;
			converter tmp1,tmp2;
			tmp1.ival = TMP_R.range(32*(k+1)-1,32*k);
			tmp2.ival = TMP_I.range(32*(k+1)-1,32*k);
			OUT_R[index] = tmp1.oval;
			OUT_I[index] = tmp2.oval;
		}
	}
}

```
### 3. Csim 

该过程仅功能仿真，以验证逻辑正确(正确则端口输出"PASS")，和实际硬件带宽无关。功能正确则进行下一步Cosim。这里给出Vitis HLS给出的综合报告，这里的资源消耗**仅包含kernel本身，而且是粗糙估计的**。
<img src = imgs/Synth_hls.png width = "720">

### 4. Cosim 接口带宽限制
Cosim运行之前需要综合，之后先运行Csim然后再Cosim，这一步之后可去Vivado里面看波形类似于RTL行为级别仿真

<img src = imgs/Cosim.png width = "700">  

若出现Csim正确，Cosim失败，则原因可分为以下几点
 - 内存越界，csim中内存越界可能不会报错。硬件越界也可以正常运行但结果会错误
 - 接口带宽，并发同时访问带宽受限的端口，导致结果未被正确传输
 - 初始化问题。HLS中使用的数组最好进行初始化或者使用输入数据进行覆盖

### 5. Vitis编译

#### 5.1. HLS源文件

将源文件fft.cpp以及fft.h拷贝到kernel文件夹内。这里需要注意，使用Makefile编译工程时，$\color{red}{顶层函数前一定要加extern "C"}$。否则kernel name会出现乱码。导致生成不了.xo文件
头文件：
```C++
extern "C" void fft(DTYPE XX_R[SIZE], DTYPE XX_I[SIZE], DTYPE OUT_R[SIZE], DTYPE OUT_I[SIZE]);
```
源文件：

```C++
extern "C" void fft(DTYPE X_R[SIZE], DTYPE X_I[SIZE], DTYPE OUT_R[SIZE], DTYPE OUT_I[SIZE])
{
#pragma HLS INTERFACE s_axilite port=return
...
}
```
#### 5.2. cfg文件编写

这里RE_ROUT 和HLS中Bundle名字一致，表示独立的axi-4接口。这里四个分别映射到四个HBM上。**直接建立端口映射，省去了以往，Vivado flow中，hls IP核导入block design中，手动例化IP连接部分**。
```C
[connectivity]
sp=fft_instance.m_axi_RE_ROUT:HBM[0]
sp=fft_instance.m_axi_RE_IOUT:HBM[1]
sp=fft_instance.m_axi_DATA_RIN:HBM[2]
sp=fft_instance.m_axi_DATA_IIN:HBM[3]
slr=fft_instance:SLR0
nk=fft:1:fft_instance
```
自动生成的block design如下图所示，对初学者还是很友好的。可见,fft_instance则是kernel实例化的名字
<img src = imgs/implement.png width="800">
>综合，布线以及功耗数据都可以在中间生成的viviado工程中找到。Implementation后的资源报告如下：
左侧:系统整体资源消耗，右侧vitis_analyzier给出的分部消耗。可以看出，在kernel较小时，系统其它资源消耗是比较大的，而且这里的kernel资源消耗和vitis_hls综合给出的差异还是比较大的。
<img src=imgs/vivado_imple.png width="800" >

#### 5.3. host 端代码编写

- 内存分配
posix_memalign(void **memptr, size_t alignment, size_t size)函数用于为设备分配page size对齐的缓冲区
reinterpret_cast强制类型转换。**申请空间最好使用该方式，可以更快的传输**OpenCl会对不是内存对齐的传输数据报Warning
	```C++
	template <typename T>
	T* aligned_alloc(std::size_t num) {
		void* ptr = NULL;
		if (posix_memalign(&ptr, 4096, num * sizeof(T))) throw std::bad_alloc();
		return reinterpret_cast<T*>(ptr);
	}
	```
- 调用kernel
  这里的很多操作都是OpenCl通用的，可以去看OpenCL编程模型。Xcl2开头的则是xilinx 扩展的OpenCL库
  

util.cpp, xcl2,cpp logger.cpp均可以复用，不用修改


#### 5.4. makefile文件

这里的makefile文件可以复用（这里仅限数据中心板子，基于HBM的；类似的ZCU版本，Xilinx GitHub库也有提供），换作其它工程基本不需要改变，直接全局搜索fft，替换为其它Vitis HLS顶层函数名字即可.这里面设定的时钟频率为300Mhz，需在makefile文件里修改。

### 6 软硬件仿真与硬件执行

- ```software emulation``` 功能验证，这里可以是多任务多线程的验证。这里host端写的比较简单
- ``hardware emulation`` kernel代码被编译成RTL硬件模块，跑在特定的模拟器上，可以得到cycle-level性能 
- ``hardware``            生成实际在FPGA上执行的二进制文件
```python
make build TARGET=sw_emu
make build TARGET=hw_emu
make build TARGET=hw
```
生成二进制文件大约1.5h。生成之后就可以硬件执行了。
```python
make run TARGET=hw
```
执行结果如下，数据量太小，扰动比较大0.09ms~0.120ms    
<img src = imgs/hw_results.png>

理论性能kernel latency如下
```5621 cycles *(10/3 ns)/1000 = 0.01874 ms```
#### 6.1 增加kernel计算量
这里将HLS 中FFT kernel内部循环10000次以观察计算耗时。反复从HBM中取数据，计算然后返回至HBM。
```191.871/10000=0.01918ms```非常接近Vitis HLS Cosim给出的latency值了。
带宽这里一定是满足的，因此计算核心的计算量对测试性能影响也很大。

<img src = imgs/fft_1024_10000.png>

### Reference

  Xilinx 官方[Tutorials](https://github.com/Xilinx/Vitis-Tutorials/tree/2020.2/Runtime_and_System_Optimization/Design_Tutorials/01-host-code-opt) 和[documentation](https://www.xilinx.com/html_docs/xilinx2020_2/vitis_doc/wzc1553475252001.html)

### TODO

- [ ] host 端直接使用OpenCL调用方式，似乎目前主流使用Xilinx自己封装的API了。
