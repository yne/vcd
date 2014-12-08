# vcd

VCD file command line viewer

## Usage

vcd [FILE] [OPTION(S)]

### FILE :
A valid .vcd file (can be passed throught the `-i` options too)

### OPTIONS :
* -h	: display the help screen
* -v=0	: verbose level (0:fatal,1:error,2:warning,3:debug)
* -w=2	: sample ascii width
* -r=2	: rounded wave (0:none,1:pipe,2:slash)
* -i=file	: input file
* -s=a,b,c	: scope(s) to display

### Note
`stdout` will be used as input stream  if no input file were providen.

So you could do :

    vcd  < in.vcd > out.txt

## Examples

    vcd sim_fifo.vcd

will output

```
43 samples / Fri Nov 21 16:56:29 2014 / 1 fs
       e_clk(!)[ 1]: ¯¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_
 
       e_rst(")[ 1]: ______/¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
 
       e_ren(#)[ 1]: ¯¯¯¯¯¯¯¯¯¯\_______/¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\___/¯¯¯\___________________/¯¯¯¯¯
 
    e_d[3:0](%)[ 4]:  Z Z Z Z Z 1 1 Z Z Z Z 2 2 3 3 4 4 5 5 Z Z 6 6 Z Z 7 7 Z Z Z Z Z Z Z Z Z Z Z Z Z Z Z
 
    e_q[3:0](&)[ 4]:  U U Z Z Z Z 0 0 1 1 Z Z Z Z Z Z Z Z Z Z Z Z Z Z Z Z 3 3 Z Z 4 4 5 5 6 6 7 7 0 0 Z Z
 
     e_empty(')[ 1]: UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU
 
       e_mid(()[ 1]: UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU
 
      e_full())[ 1]: UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU
 
+-- fifo1
|        clk(*)[ 1]: ¯¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_/¯\_
|
|        rst(+)[ 1]: ______/¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
|
|        ren(,)[ 1]: ¯¯¯¯¯¯¯¯¯¯\_______/¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\___/¯¯¯\___________________/¯¯¯¯¯
|
|        wen(-)[ 1]: ¯¯¯¯¯¯¯¯¯¯\___/¯¯¯¯¯¯¯\_______________/¯¯¯\___/¯¯¯\___/¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
|
|     d[3:0](.)[ 4]:  Z Z Z Z Z 1 1 Z Z Z Z 2 2 3 3 4 4 5 5 Z Z 6 6 Z Z 7 7 Z Z Z Z Z Z Z Z Z Z Z Z Z Z Z
|
|     q[3:0](/)[ 4]:  U U Z Z Z Z 0 0 1 1 Z Z Z Z Z Z Z Z Z Z Z Z Z Z Z Z 3 3 Z Z 4 4 5 5 6 6 7 7 0 0 Z Z
|
|      empty(0)[ 1]: UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU
|
|        mid(1)[ 1]: UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU
|
|       full(2)[ 1]: UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU
|
| w_adr[1:0](3)[ 2]:  U U 0 0 0 0 1 1 1 1 1 1 2 2 3 3 0 0 1 1 1 1 2 2 2 2 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3
|
| r_adr[1:0](4)[ 2]:  U U 0 0 0 0 0 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 2 2 2 2 3 3 3 3 0 0 1 1 2 2 3 3 3 3 3 3
|
       e_wen(^)[ 1]: ____________________________________________________________________________________
```

    vcd -i=sim_fifo.vcd -w=1 -r=0 -s=fifo1

will output only the fifo1 module in a smaller graph without transitions

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