# pcdrv_testapp_psyq
Playstation1 'pcdrv' file server app using unirom + nops. (PSYQ version)

For the nugget (non-psyq) version, see here: https://github.com/JonathanDotCel/pcdrv_testapp_nugget

I'll keep this short as it's not meant to be an intro to psx programming or psqy in general.
If you're after that sort of thing, come say hi on the psxdev discord, or see here:
- http://psx.arthus.net/starting.html
- https://ps1.consoledev.net/

## Doin a compile

- have psyq in c:\psyq
- grab the 2mbyte.obj from anywhere in your sdk dir and stick it in the folder
- run buildme.bat

## What do?

### Put unirom in debug mode
`nops /debug` or `l1 + square`
The installs unirom's kernel mode handler, so you can do sio stuff while games/apps are running.

### Upload via unirom & monitor
`nops /fast /upload pcdrvtest.exe /m`

the `/m` part leaves the monitor open for the pcdrv stuff

## Where's the `pcdrv:` or `sim:` virtual device?

Uinirom won't provide it due to kernel space limitations.
I'll write some code for that if there's interest.


