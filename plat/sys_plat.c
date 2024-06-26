#include "sys_plat.h"

#if defined(SYS_PLAT_X86OS)
#include "dev/time.h"
#include "os_cfg.h"
#include "dbg.h"
#include "sys.h"
#include "mblock.h"
#include "cpu/irq.h"
#include "core/task.h"

#define NET_TASK_NR                 3           // task number
#define NET_SEM_NR                  100         // sem num
#define NET_MUTEX_NR                100         // mutex num

typedef struct _net_task_t {
    task_t task;
    uint32_t stack[10*1024];
}net_task_t;

static net_task_t task_tbl[NET_TASK_NR];
static mblock_t task_mblock;
static sem_t sem_tbl[NET_SEM_NR];
static mblock_t sem_mblock;
static mutex_t mutex_tbl[NET_MUTEX_NR];
static mblock_t mutex_mblock;

void sys_time_curr (net_time_t * time) {
    *time = sys_get_ticks();
}

int sys_time_goes (net_time_t * pre) {
    // get current time
    net_time_t curr = sys_get_ticks();

    // diff ms
   int diff_ms = (curr - *pre) * OS_TICK_MS;

    // set current time
    *pre  = curr;
    return diff_ms;    
}

// create semaphore
sys_sem_t sys_sem_create(int init_count) {
    sys_sem_t sem = (sys_sem_t)mblock_alloc(&sem_mblock, -1);
    if (sem != SYS_SEM_INVALID) {
       sem_init(sem, init_count);
    }
    return sem;
}

void sys_sem_free(sys_sem_t sem) {
    mblock_free(&sem_mblock, sem);
}

int sys_sem_wait(sys_sem_t sem, uint32_t ms) {
    return sem_wait_tmo(sem, ms);
}

void sys_sem_notify(sys_sem_t sem) {
    sem_notify(sem);
}

// create mutex
sys_mutex_t sys_mutex_create(void) {
    sys_mutex_t m = (sys_mutex_t)mblock_alloc(&mutex_mblock, -1);
    if (m != SYS_MUTEx_INVALID) {
        mutex_init(m);
    }
    return m;
}

void sys_mutex_free(sys_mutex_t mutex) {
    mblock_free(&mutex_mblock, mutex);
}

void sys_mutex_lock(sys_mutex_t mutex) {
    mutex_lock(mutex);
}

void sys_mutex_unlock(sys_mutex_t mutex) {
    mutex_unlock(mutex);
}

sys_intlocker_t sys_intlocker_lock(void) {
    return irq_enter_protection();
}

void sys_intlocker_unlock(sys_intlocker_t locker) {
    irq_leave_protection(locker);
}

sys_thread_t sys_thread_create(sys_thread_func_t entry, void* arg) {
    net_task_t * task = (net_task_t *)mblock_alloc(&task_mblock, -1);

    int err = task_init(&task->task, "net task", TASK_FLAG_SYSTEM, (uint32_t)entry, (uint32_t)(task->stack + sizeof(task->stack)));
    if (err < 0) {
        log_printf("net: create task failed.");
        return SYS_THREAD_INVALID;
    }

    task_start(&task->task);
    return &task->task;
}

void sys_thread_exit (int error) {

}

sys_thread_t sys_thread_self (void) {
    return task_current();
}

void sys_sleep(int ms) {
    sys_msleep(ms);
}

void sys_plat_init(void) {
    mblock_init(&task_mblock, task_tbl, sizeof(net_task_t), NET_TASK_NR, NLOCKER_NONE);
    mblock_init(&sem_mblock, sem_tbl, sizeof(sem_t), NET_SEM_NR, NLOCKER_NONE);
    mblock_init(&mutex_mblock, mutex_tbl, sizeof(mutex_t), NET_MUTEX_NR, NLOCKER_NONE);
}
	
 // windows
#elif defined(SYS_PLAT_WINDOWS)

#include <tchar.h>
#include <time.h>
#include "pcap.h"

#pragma comment(lib, "ws2_32.lib")  // load win32 net lib

/**
 * load pcap lib
 */
int load_pcap_lib() {
    static int dll_loaded = 0;
    _TCHAR  npcap_dir[512];
    int size;

    if (dll_loaded) {
        return 0;
    }

    size = GetSystemDirectory(npcap_dir, 480);
    if (!size) {
        fprintf(stderr, "Error in GetSystemDirectory: %x", GetLastError());
        return -1;
    }

    _tcscat_s(npcap_dir, 512, _T("\\Npcap"));
    if (SetDllDirectory(npcap_dir) == 0) {
        fprintf(stderr, "Error in SetDllDirectory: %x", GetLastError());
        return -1;
    }

    dll_loaded = 1;
    return 0;
}

/**
 * get current time
 */
void sys_time_curr (net_time_t * time)
{
    // https://learn.microsoft.com/zh-cn/windows/win32/api/sysinfoapi/nf-sysinfoapi-gettickcount?redirectedfrom=MSDN
    *time = GetTickCount();
}

/**
 * Returns the time difference between the current time and the incoming time,
 * and the time is updated to the current time after the call is completed
 */
int sys_time_goes (net_time_t * pre)
{
    net_time_t curr = GetTickCount();

   int diff_ms = curr - *pre;

    *pre  = curr;
    return diff_ms;
}

sys_sem_t sys_sem_create(int init_count) {
    return CreateSemaphore(NULL, init_count, 0xFFFF, NULL);
}

void sys_sem_free(sys_sem_t sem) {
    CloseHandle(sem);
}

int sys_sem_wait(sys_sem_t sem, uint32_t tmo_ms) {
    DWORD tmo = (tmo_ms == 0) ? INFINITE : tmo_ms;
    DWORD err = WaitForSingleObject(sem, tmo);
    if (err == WAIT_OBJECT_0) {
        return 0;
    }

    if (err == WAIT_TIMEOUT) {
        return -4;
    }

    printf("wait error\n");
    return -1;
}

void sys_sem_notify(sys_sem_t sem) {
    ReleaseSemaphore(sem, 1, NULL);
}

/**
 * create thread mutex
 */
sys_mutex_t sys_mutex_create(void) {
    sys_mutex_t mutex = CreateMutex(NULL, FALSE, NULL); // 初始不被占用
    if (mutex == NULL) {
        return SYS_MUTEX_INVALID;
    }
    return mutex;
}

/**
 * free thread mutex
 */
void sys_mutex_free(sys_mutex_t locker) {
    ReleaseMutex(locker);
}

/**
 * lock
 */
void sys_mutex_lock(sys_mutex_t locker) {
    WaitForSingleObject(locker, INFINITE);
}

/**
 * unlock
 */
void sys_mutex_unlock(sys_mutex_t locker) {
    ReleaseMutex(locker);
}

sys_thread_t sys_thread_create(void (*entry)(void * arg), void * arg) {
    return CreateThread(
        NULL,                           // SD
        0,                              // initial stack size
        (LPTHREAD_START_ROUTINE)entry,  // thread function
        arg,                            // thread argument
        0,                              // run after created
        NULL                            // thread identifier
        );
}

/**
 * exit thread
 */
void sys_thread_exit (int error) {
    ExitThread((DWORD)error);
}

sys_thread_t sys_thread_self (void) {
    return GetCurrentThread();
}

/**
 * sleep
 */
void sys_sleep(int ms) {
    Sleep(ms);
}

void sys_plat_init(void) {
}

#elif defined(SYS_PLAT_LINUX) || defined(SYS_PLAT_MAC)
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include "pcap.h"

#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/time.h>

int load_pcap_lib(void) {
    return 0;
}

/**
 * get current time
 */
void sys_time_curr (net_time_t * time) {
    gettimeofday(time, NULL);
}

int sys_time_goes (net_time_t * pre) {
    struct timeval curr;
    gettimeofday(&curr, NULL);

    int diff_ms = (curr.tv_sec - pre->tv_sec) * 1000 + (curr.tv_usec - pre->tv_usec) / 1000;

    *pre  = curr;
    return diff_ms;
}

sys_sem_t sys_sem_create(int init_count) {
    sys_sem_t sem = (sys_sem_t)malloc(sizeof(struct _xsys_sem_t));
    if (!sem) {
        return (sys_sem_t)0;
    }

    sem->count = init_count;

    int err = pthread_cond_init(&(sem->cond), NULL);
    if (err) {
        return (sys_sem_t)0;
    }

    err = pthread_mutex_init(&(sem->locker), NULL);
    if (err) {
        return (sys_sem_t)0;
    }

    return sem;
}

void sys_sem_free(sys_sem_t sem) {
    pthread_cond_destroy(&(sem->cond));
    pthread_mutex_destroy(&(sem->locker));
    free(sem);
}

int sys_sem_wait(sys_sem_t sem, uint32_t tmo_ms) {
    pthread_mutex_lock(&(sem->locker));

    if (sem->count <= 0) {
        int ret;

        if (tmo_ms > 0) {
            struct timespec ts;
            ts.tv_nsec = (tmo_ms % 1000) * 1000000L;
            ts.tv_sec = time(NULL) + tmo_ms / 1000;
            ret = pthread_cond_timedwait(&sem->cond, &sem->locker, &ts);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&(sem->locker));
                return -1;
            }
        } else {
            ret = pthread_cond_wait(&sem->cond, &sem->locker);
            if (ret < 0) {
                pthread_mutex_unlock(&(sem->locker));
                return -1;
            }
        }
    }

    sem->count--;
    pthread_mutex_unlock(&(sem->locker));
    return 0;
}

void sys_sem_notify(sys_sem_t sem) {
    pthread_mutex_lock(&(sem->locker));

    sem->count++;

    pthread_cond_signal(&(sem->cond));

    pthread_mutex_unlock(&(sem->locker));
}

sys_thread_t sys_thread_create(void (*entry)(void * arg), void* arg) {
    pthread_t pthread;

    int err = pthread_create(&pthread,
                         NULL,
                         (void* (*)(void * arg))entry,
                         arg);
    if (err) {
        return (pthread_t)0;
    }

    return pthread;
}

void sys_thread_del_self() {
    pthread_exit(NULL);
}

void sys_sleep(int ms) {
    usleep(1000 * ms);
}

sys_mutex_t sys_mutex_create(void) {
    sys_mutex_t mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));

    int err = pthread_mutex_init(mutex, NULL);
    if (err < 0) {
        return (sys_mutex_t)0;
    }
    return mutex;
}

void sys_mutex_free(sys_mutex_t locker) {
    pthread_mutex_destroy(locker);
    free(locker);
}

void sys_mutex_lock(sys_mutex_t locker) {
    pthread_mutex_lock(locker);
}

void sys_mutex_unlock(sys_mutex_t locker) {
    pthread_mutex_unlock(locker);
}


void sys_thread_exit (int error) {
}

sys_thread_t sys_thread_self (void) {
    return pthread_self();
}

void sys_plat_init(void) {
}

#endif

#if defined(NET_DRIVER_PCAP)
#include <pcap.h>

/**
 * find local net interface by ip address
 */
int pcap_find_device(const char* ip, char* name_buf) {
    struct in_addr dest_ip;

    inet_pton(AF_INET, ip, &dest_ip);

    // get all of local net interface
    char err_buf[PCAP_ERRBUF_SIZE];
    pcap_if_t* pcap_if_list = NULL;
    int err = pcap_findalldevs(&pcap_if_list, err_buf);
    if (err < 0) {
        pcap_freealldevs(pcap_if_list);
        return -1;
    }

    pcap_if_t* item;
    for (item = pcap_if_list; item != NULL; item = item->next) {
        if (item->addresses == NULL) {
            continue;
        }

        for (struct pcap_addr* pcap_addr = item->addresses; pcap_addr != NULL; pcap_addr = pcap_addr->next) {
            struct sockaddr* sock_addr = pcap_addr->addr;
            if (sock_addr->sa_family != AF_INET) {
                continue;
            }

            struct sockaddr_in* curr_addr = ((struct sockaddr_in*)sock_addr);
            if (curr_addr->sin_addr.s_addr == dest_ip.s_addr) {
                strcpy(name_buf, item->name);
                pcap_freealldevs(pcap_if_list);
                return 0;
            }
        }
    }

    pcap_freealldevs(pcap_if_list);
    return -1;
}

/*
 * show all of local net interface
 */
int pcap_show_list(void) {
    char err_buf[PCAP_ERRBUF_SIZE];
    pcap_if_t* pcapif_list = NULL;
    int count = 0;

    int err = pcap_findalldevs(&pcapif_list, err_buf);
    if (err < 0) {
        fprintf(stderr, "scan net card failed: %s\n", err_buf);
        pcap_freealldevs(pcapif_list);
        return -1;
    }

    printf("net card list: \n");

    for (pcap_if_t* item = pcapif_list; item != NULL; item = item->next) {
        if (item->addresses == NULL) {
            continue;
        }

        // 显示ipv4地址
        for (struct pcap_addr* pcap_addr = item->addresses; pcap_addr != NULL; pcap_addr = pcap_addr->next) {
            char str[INET_ADDRSTRLEN];
            struct sockaddr_in* ip_addr;

            struct sockaddr* sockaddr = pcap_addr->addr;
            if (sockaddr->sa_family != AF_INET) {
                continue;
            }

            ip_addr = (struct sockaddr_in*)sockaddr;
            char * name = item->description;
            if (name == NULL) {
                name = item->name;
            }
            printf("%d: IP:%s name: %s, \n",
                count++,
                name ? name : "unknown",
                inet_ntop(AF_INET, &ip_addr->sin_addr, str, sizeof(str))
            );
            break;
        }
    }

    pcap_freealldevs(pcapif_list);

    if ((pcapif_list == NULL) || (count == 0)) {
        fprintf(stderr, "error: no net card!\n");
        return -1;
    }

    printf("no net card found, check system configuration\n");
    return 0;
}

/**
 * open pcap dev interface
 */
pcap_t * pcap_device_open(const char* ip, const uint8_t* mac_addr) {
    // load pcap lib
    if (load_pcap_lib() < 0) {
        fprintf(stderr, "load pcap lib error。please isntall npcap.dll\n");
        return (pcap_t *)0;
    }

    char name_buf[256];
    if (pcap_find_device(ip, name_buf) < 0) {
        fprintf(stderr, "pcap find error: no net card has ip: %s. \n", ip);
        pcap_show_list();
        return (pcap_t*)0;
    }

    // get ip、netmask
    char err_buf[PCAP_ERRBUF_SIZE];
    bpf_u_int32 mask;
    bpf_u_int32 net;
    if (pcap_lookupnet(name_buf, &net, &mask, err_buf) == -1) {
        printf("pcap_lookupnet error: no net card: %s\n", name_buf);
        net = 0;
        mask = 0;
    }

    // open device
    pcap_t * pcap = pcap_create(name_buf, err_buf);
    if (pcap == NULL) {
        fprintf(stderr, "pcap_create: create pcap failed %s\n net card name: %s\n", err_buf, name_buf);
        fprintf(stderr, "Use the following:\n");
        pcap_show_list();
        return (pcap_t*)0;
    }

    if (pcap_set_snaplen(pcap, 65536) != 0) {
        fprintf(stderr, "pcap_open: set none block failed: %s\n", pcap_geterr(pcap));
        return (pcap_t*)0;
    }

    if (pcap_set_promisc(pcap, 1) != 0) {
        fprintf(stderr, "pcap_open: set none block failed: %s\n", pcap_geterr(pcap));
        return (pcap_t*)0;
    }

    if (pcap_set_timeout(pcap, 0) != 0) {
        fprintf(stderr, "pcap_open: set none block failed: %s\n", pcap_geterr(pcap));
        return (pcap_t*)0;
    }

    if (pcap_set_immediate_mode(pcap, 1) != 0) {
        fprintf(stderr, "pcap_open: set im block failed: %s\n", pcap_geterr(pcap));
        return (pcap_t*)0;
    }

    if (pcap_activate(pcap) != 0) {
        fprintf(stderr, "pcap_open: active failed: %s\n", pcap_geterr(pcap));
        return (pcap_t*)0;
    }

    if (pcap_setnonblock(pcap, 0, err_buf) != 0) {
        fprintf(stderr, "pcap_open: set none block failed: %s\n", pcap_geterr(pcap));
        return (pcap_t*)0;
    }

    char filter_exp[256];
    struct bpf_program fp;
    sprintf(filter_exp,
        "(ether dst %02x:%02x:%02x:%02x:%02x:%02x or ether broadcast) and (not ether src %02x:%02x:%02x:%02x:%02x:%02x)",
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    if (pcap_compile(pcap, &fp, filter_exp, 0, net) == -1) {
        printf("pcap_open: couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(pcap));
        return (pcap_t*)0;
    }
    if (pcap_setfilter(pcap, &fp) == -1) {
        printf("pcap_open: couldn't install filter %s: %s\n", filter_exp, pcap_geterr(pcap));
        return (pcap_t*)0;
    }
    return pcap;
}

#endif

