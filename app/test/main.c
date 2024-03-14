#include <stdio.h>
#include "net.h"
#include <sys_plat.h>


int main()
{
    net_init();
    net_start();

    for(;;){ sys_sleep(10);}
    return 0;
}