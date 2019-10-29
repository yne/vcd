<img src=https://svgur.com/i/Fck.svg width=100% height=200>

VCD file command line viewer

Lightweight command line remplacement to GTKWave (16801 KB vs 6 KB)

## Download

Windows, Linux and MacOS binaries are available in the [releases page](../../releases)

## Usage

```
Usage: vcd [OPTION]... [FILE]

 -h     : display this help screen
 -v=0   : verbose level (0:fatal,1:error,2:warning,3:debug)
 -w=2   : sample ascii width (1,2,...)
 -r=2   : rounded wave (0:none,1:pipe,2:slash)
 -c=32  : name column width
 -s=a,b : comma separated scopes to display
```

## Examples

```bash
vcd sim_fifo.vcd
vcd  < in.vcd > out.txt
vcd -i=sim_fifo.vcd -w=1 -r=0 -s=fifo1
```

# Preview

```
43 samples / Fri Nov 21 16:56:29 2014 / 1 fs
+-- fifo1
|        clk(*)[ 1]: ¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯_¯  
|        rst(+)[ 1]: ___¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯  
|        ren(,)[ 1]: ¯¯¯¯¯____¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯__¯¯__________¯¯¯¯  
|        wen(-)[ 1]: ¯¯¯¯¯__¯¯¯¯________¯¯__¯¯__¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯  
|     d[3:0](.)[ 4]: ZZZZZ11ZZZZ22334455ZZ66ZZ77ZZZZZZZZZZZZZZZZ  
|     q[3:0](/)[ 4]: UUZZZZ0011ZZZZZZZZZZZZZZZZ33ZZ4455667700ZZZ  
|      empty(0)[ 1]: UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU  
|        mid(1)[ 1]: UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU  
|       full(2)[ 1]: UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU  
| w_adr[1:0](3)[ 2]: UU00001111112233001111222233333333333333333  
| r_adr[1:0](4)[ 2]: UU00000011111111111111222233330011223333333
```
