
del *.sym
del *.cpe
del *.map
del *.elf
del *.o

call c:\psyq\pspaths.bat

REM 2mbyte.obj is peppered through your psyq directory
REM put it on your path or drop it in this folder

ccpsx -O3 -Xo$80010000 main.c timloader.c gpu.c 2mbyte.obj -opcdrvtest.cpe,pcdrvtest.sym,pcdrvtest.map

cpe2x pcdrvtest.cpe

