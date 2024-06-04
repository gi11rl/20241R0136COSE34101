#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#define PROCESS_NUM 5
#define TIME_QUANTUM 3
#define MAX_CPU_BURST_TIME 10
#define MAX_IO_BURST_TIME 5
#define MAX_ARRIVAL_TIME 10
#define MAX_PRIORITY 5

typedef struct {
    int pid;
    int arrival_time;
    int priority;
    int cpu_burst_time;
    int io_burst_time;
    int io_start;           // 몇 초 남았을 때 시작할지
                            // 출력되는 io_start 값은 몇 초 지났을 때 시작할지

    int waiting_queue_time; // waiting queue에 머무르는 시간 
                            // waiting time에서 제외하기 위함

    int terminate_time;     // 프로세스 종료 시각
    int turnaround_time;
    int waiting_time;
} Process;


typedef struct {
    Process processes[PROCESS_NUM];
    int count;
} Queue;

// pid 중복 여부 검사
int is_pid_duplicate(int pid, Process *processes, int n) {
    for (int i = 0; i < n; i++) {
        if (processes[i].pid == pid) {
            return 1;
        }
    }
    return 0;
}


void create_processes(Process *processes, int n) {
    srand(time(NULL));
    int pid = 0;

    for (int i = 0; i < n; i++) {
        // pid : 0 ~ PROCESS_NUM-1
        // pid는 random, unique
        while (1) {
            pid = rand() % PROCESS_NUM;

            if (!is_pid_duplicate(pid, processes, i)) {
                processes[i].pid = pid;
                break; 
            }   
        }

        // arrival time : 0 ~ MAX_ARRIVAL_TIME-1
        processes[i].arrival_time = rand() % MAX_ARRIVAL_TIME;
        // cpu burst time : 1 ~ MAX_CPU_BURST_TIME
        processes[i].cpu_burst_time = rand() % MAX_CPU_BURST_TIME + 1;

        // io start time : 1 ~ cpu burst time-1
        // io start time = 0 -> "no io operation"
        if (processes[i].cpu_burst_time == 1) { // cpu burst time = 1 -> io operation (x)
            processes[i].io_start = 0;
            processes[i].io_burst_time = 0;
        } 
        
        else { 
            processes[i].io_start = rand() % processes[i].cpu_burst_time;
        }

        // io burst time : 1 ~ MAX_IO_BURST_TIME
        // io burst time = 0 -> "no io operation"
        if (processes[i].io_start == 0) { // io start time = 0 -> "no io operation"
            processes[i].io_burst_time = 0;
        }

        else { 
            processes[i].io_burst_time = rand() % (MAX_IO_BURST_TIME + 1);
        }

        // priority : 1 ~ MAX_PRIORITY
        // 숫자가 작을수록 우선순위가 높다
        processes[i].priority = rand() % MAX_PRIORITY + 1;

        // waiting queue time, terminate time, turnaround time, waiting time 초기화
        processes[i].waiting_queue_time = 0;
        processes[i].terminate_time = 0;
        processes[i].turnaround_time = 0;
        processes[i].waiting_time = 0;
    }
}

// pid에 대해 오름차순으로 정렬
// bubble sort
void sort_processes_pid(Process *processes, int n) {
    Process temp = {0};
    for (int i = 0; i < n - 1; i++)
    {
        for (int j = 0; j < n - 1 - i; j++)
        {
            if (processes[j].pid > processes[j + 1].pid)
            {
                temp = processes[j];
                processes[j] = processes[j + 1];
                processes[j + 1] = temp;
            }
        }
    }
}

// cpu burst time에 대해 오름차순으로 정렬
// bubble sort
void sort_processes_time(Process *processes, int n) {
    Process temp = {0};
    for (int i = 0; i < n - 1; i++)
    {
        for (int j = 0; j < n - 1 - i; j++)
        {
            if (processes[j].cpu_burst_time > processes[j + 1].cpu_burst_time)
            {
                temp = processes[j];
                processes[j] = processes[j + 1];
                processes[j + 1] = temp;
            }
        }
    }
}

// priority에 대해 오름차순으로 정렬
// 숫자가 작을수록 우선순위가 높다
// bubble sort
void sort_processes_priority(Process *processes, int n) {
    Process temp = {0};
    for (int i = 0; i < n - 1; i++)
    {
        for (int j = 0; j < n - 1 - i; j++)
        {
            if (processes[j].priority > processes[j + 1].priority)
            {
                temp = processes[j];
                processes[j] = processes[j + 1];
                processes[j + 1] = temp;
            }
        }
    }
}

// queue 초기화
void config_queues(Queue *ready_queue, Queue *waiting_queue, Queue *terminated_queue) {
    ready_queue->count = 0;
    waiting_queue->count = 0;
    terminated_queue->count = 0;
}

// queue 기능 : push
void push_queue(Queue *queue, Process p) {
    queue->processes[queue->count++] = p;
}

// queue 기능 : pop
Process pop_queue(Queue *queue) {
    Process p = queue->processes[0];
    for (int i=0; i<queue->count -1; i++) {
        queue->processes[i] = queue->processes[i+1];
    }
    queue->count--;
    return p;
}

// FCFS
// queue에 먼저 들어온 순으로 CPU 할당
// 동시에 들어오면 PID 가 작은 순으로 CPU 할당
Queue FCFS(Process *job_queue, int n) {

    // 간트 차트 출력을 구성하는 문자열
    char gantt_chart1[200] = {0};
    char gantt_chart2[200] = {0};
    char gantt_line[200] = {0};

    int time = 0;       // 시각
    int CPU_busy = 0;   // CPU_busy = 1 -> CPU 사용 중
    int IO_busy = 0;    // IO_busy = 1 -> IO device 사용 중

    // queue 선언
    Queue ready_queue;
    Queue waiting_queue;
    Queue terminated_queue; // terminated 된 프로세스 저장
    config_queues(&ready_queue, &waiting_queue, &terminated_queue); // queue 초기화

    // running, IO 프로세스 초기화
    Process running = {0}; // 현재 CPU 를 할당 받은 프로세스
    Process IO = {0};      // 현재 IO device 를 할당 받은 프로세스


    // 모든 프로세스가 terminate 되면 알고리즘 종료
    for (time = 0; terminated_queue.count < n; time++) {

        // IO terminate
        if (IO_busy && IO.io_burst_time<=0) {
            push_queue(&ready_queue, IO);
            IO_busy=0;
        }

        // job queue -> ready queue
        for (int i=0; i<n; i++) {
            if (job_queue[i].arrival_time == time) {
                push_queue(&ready_queue, job_queue[i]);
            }
            // arrival time이 같을 경우 PID 순으로 가져온다 
            // - 이미 job_queue가 pid 순으로 정렬되어 있기 때문
        }

        // CPU running 프로세스 -> IO operation
        if (CPU_busy && running.io_burst_time != 0 && running.cpu_burst_time == running.io_start) {
            // running 프로세스가 있고, IO operation을 수행하는 프로세스고, 현재 IO start time이면
            running.waiting_queue_time = time;   // waiting queue 에 머무르는 시간을 waiting time에서 제외하기 위해 기록
            push_queue(&waiting_queue, running); // waiting queue에 push
            CPU_busy=0;                          // CPU 사용 중 아님
        }

        // ready queue -> CPU running
        if (!CPU_busy && ready_queue.count > 0) {
            running = pop_queue(&ready_queue);
            running.cpu_burst_time--;
            
            // 간트 차트 출력
            char str[10];
            sprintf(str, "====");
            strcat(gantt_line, str);
            sprintf(str, "|P%2d", running.pid);
            strcat(gantt_chart1, str);
            sprintf(str, "%-4d", time);
            strcat(gantt_chart2, str);
            
            CPU_busy = 1;
        }  

        // idle
        else if (!CPU_busy && ready_queue.count <= 0) {
            char str[10];
            sprintf(str, "=====");
            strcat(gantt_line, str);
            sprintf(str, "|idle");
            strcat(gantt_chart1, str);
            sprintf(str, "%-5d", time);
            strcat(gantt_chart2, str);
        }

        // CPU running continue
        else {
            char str[10];
            
            sprintf(str, "====");
            strcat(gantt_line, str);
            sprintf(str, "|P%2d", running.pid);
            strcat(gantt_chart1, str);
            sprintf(str, "%-4d", time);
            strcat(gantt_chart2, str);
            
            running.cpu_burst_time--;
        }

        // terminate
        if (CPU_busy && running.cpu_burst_time <= 0) {
            CPU_busy=0;
            running.terminate_time = time+1;
            push_queue(&terminated_queue, running);
        }

    
        // waiting queue -> IO
        if (!IO_busy && waiting_queue.count > 0) {
            IO = pop_queue(&waiting_queue);
            IO.waiting_queue_time = time - IO.waiting_queue_time; // waiting queue에 머무르는 시간 제외 용도
            IO.io_burst_time--;
            IO_busy=1;
        }

        // IO continue
        else if (IO_busy) {
            IO.io_burst_time--;
        }

    }

    // 간트 차트 출력
    char str[10];
    sprintf(str, "=");
    strcat(gantt_line, str);
    sprintf(str, "|");
    strcat(gantt_chart1, str);
    sprintf(str, "%-5d", time);
    strcat(gantt_chart2, str);
    printf("%s\n%s\n%s\n%s\n", gantt_line, gantt_chart1, gantt_line, gantt_chart2);

    return terminated_queue;    // terminated_queue 반환
}


// non-preemptive SJF
// remaining time 이 짧은 순으로 CPU 할당
// CPU 를 빼앗을 수 없다
Queue Non_Preemptive_SJF(Process *job_queue, int n) {

    char gantt_chart1[200] = {0};
    char gantt_chart2[200] = {0};
    char gantt_line[200] = {0};

    int time = 0;
    int CPU_busy = 0;
    int IO_busy = 0;

    Queue ready_queue;
    Queue waiting_queue;
    Queue terminated_queue;
    config_queues(&ready_queue, &waiting_queue, &terminated_queue);

    Process running = {0};
    Process IO = {0};

    for (time = 0; terminated_queue.count < n; time++) {
        // IO terminate
        if (IO_busy && IO.io_burst_time <= 0) {
            push_queue(&ready_queue, IO);
            IO_busy = 0;
        }

        // job queue -> ready queue
        for (int i = 0; i < n; i++) {
            if (job_queue[i].arrival_time == time) {
                push_queue(&ready_queue, job_queue[i]);
            }
        }

        // IO terminate, job queue -> ready queue
        // cpu burst time 에 대해 오름차순으로 정렬 
        sort_processes_time(ready_queue.processes, ready_queue.count);


        // CPU running 프로세스 -> IO operation
        if (CPU_busy && running.io_burst_time != 0 && running.cpu_burst_time == running.io_start) {
            running.waiting_queue_time = time;
            push_queue(&waiting_queue, running);
            CPU_busy = 0;
        }

        // ready queue -> CPU running
        if (!CPU_busy && ready_queue.count > 0) {
            running = pop_queue(&ready_queue);
            running.cpu_burst_time--;

            char str[10];
            sprintf(str, "====");
            strcat(gantt_line, str);
            sprintf(str, "|P%2d", running.pid);
            strcat(gantt_chart1, str);
            sprintf(str, "%-4d", time);
            strcat(gantt_chart2, str);

            CPU_busy = 1;
        } 
        
        // idle
        else if (!CPU_busy && ready_queue.count <= 0) {
            char str[10];
            sprintf(str, "=====");
            strcat(gantt_line, str);
            sprintf(str, "|idle");
            strcat(gantt_chart1, str);
            sprintf(str, "%-5d", time);
            strcat(gantt_chart2, str);
        } 
        
        // CPU running continue
        else {
            char str[10];
            sprintf(str, "====");
            strcat(gantt_line, str);
            sprintf(str, "|P%2d", running.pid);
            strcat(gantt_chart1, str);
            sprintf(str, "%-4d", time);
            strcat(gantt_chart2, str);

            running.cpu_burst_time--;
        }

        // terminate
        if (CPU_busy && running.cpu_burst_time <= 0) {
            CPU_busy = 0;
            running.terminate_time = time+1;
            push_queue(&terminated_queue, running);
        }

        // waiting queue -> IO
        if (!IO_busy && waiting_queue.count > 0) {
            IO = pop_queue(&waiting_queue);
            IO.waiting_queue_time = time - IO.waiting_queue_time;
            IO.io_burst_time--;
            IO_busy = 1;
        } 
        
        // IO continue
        else if (IO_busy) {
            IO.io_burst_time--;
        }

    }

    char str[10];
    sprintf(str, "=");
    strcat(gantt_line, str);
    sprintf(str, "|");
    strcat(gantt_chart1, str);
    sprintf(str, "%-5d", time);
    strcat(gantt_chart2, str);
    printf("%s\n%s\n%s\n%s\n", gantt_line, gantt_chart1, gantt_line, gantt_chart2);
    
    return terminated_queue;
}


// preemptive SJF
// remaining time 이 짧은 순으로 CPU 할당
// CPU 를 빼앗을 수 있다
Queue Preemptive_SJF(Process *job_queue, int n) {
    char gantt_chart1[200] = {0};
    char gantt_chart2[200] = {0};
    char gantt_line[200] = {0};

    int time = 0;
    int CPU_busy = 0;
    int IO_busy = 0;

    Queue ready_queue;
    Queue waiting_queue;
    Queue terminated_queue;
    config_queues(&ready_queue, &waiting_queue, &terminated_queue);

    Process running = {0};
    Process IO = {0};

    for (time = 0; terminated_queue.count < n; time++) {
        // IO terminate
        if (IO_busy && IO.io_burst_time <= 0) {
            push_queue(&ready_queue, IO);
            IO_busy = 0;
        }

        // job queue -> ready queue
        for (int i = 0; i < n; i++) {
            if (job_queue[i].arrival_time == time) {
                push_queue(&ready_queue, job_queue[i]);
            }
        }

        // IO terminate, job queue -> ready queue
        // cpu burst time 에 대해 오름차순으로 정렬
        sort_processes_time(ready_queue.processes, ready_queue.count);


        // CPU running 프로세스 -> IO operation
        if (CPU_busy && running.io_burst_time != 0 && running.cpu_burst_time == running.io_start) {
            running.waiting_queue_time = time;
            push_queue(&waiting_queue, running);
            CPU_busy = 0;
        }

        // running 프로세스가 교체되는 case 1 : ready queue -> running
        if (!CPU_busy && ready_queue.count > 0) {
            running = pop_queue(&ready_queue);
            CPU_busy = 1;
        } 
        

        // running 프로세스가 교체되는 case 2 : preemptive
        else if (CPU_busy && ready_queue.count > 0 && ready_queue.processes[0].cpu_burst_time < running.cpu_burst_time) {
            // ready queue에서 remaining time이 가장 짧은 프로세스가 running 이 된 것
            // 그러므로 new arrival이 없었다면 ready queue에 running보다 짧은 remaining time 이 존재할 수 없다
            // 즉, ready queue에 running보다 짧은 remaining time 이 존재한다는 것은 running을 대체할 new arrival이 발생했다는 것
            Process temp = running;
            running = pop_queue(&ready_queue);
            push_queue(&ready_queue, temp);
        }
        
        // CPU running continue
        if (CPU_busy) {
            running.cpu_burst_time--;

            char str[10];
            sprintf(str, "====");
            strcat(gantt_line, str);
            sprintf(str, "|P%2d", running.pid);
            strcat(gantt_chart1, str);
            sprintf(str, "%-4d", time);
            strcat(gantt_chart2, str);
        } 
        
        // idle
        else {
            char str[10];
            sprintf(str, "=====");
            strcat(gantt_line, str);
            sprintf(str, "|idle");
            strcat(gantt_chart1, str);
            sprintf(str, "%-5d", time);
            strcat(gantt_chart2, str);
        }

        // terminate
        if (CPU_busy && running.cpu_burst_time <= 0) {
            CPU_busy = 0;
            running.terminate_time = time+1;
            push_queue(&terminated_queue, running);
        }

        // waiting queue -> IO
        if (!IO_busy && waiting_queue.count > 0) {
            
            IO = pop_queue(&waiting_queue);
            IO.waiting_queue_time = time - IO.waiting_queue_time;
            IO.io_burst_time--;
            IO_busy = 1;
        } 
        
        // IO continue
        else if (IO_busy) {
            IO.io_burst_time--;
        }

    }

    char str[10];
    sprintf(str, "=");
    strcat(gantt_line, str);
    sprintf(str, "|");
    strcat(gantt_chart1, str);
    sprintf(str, "%-5d", time);
    strcat(gantt_chart2, str);
    printf("%s\n%s\n%s\n%s\n", gantt_line, gantt_chart1, gantt_line, gantt_chart2);

    return terminated_queue;
}

// non-preemptive priority
// priority가 높은(=숫자가 작은) 순으로 CPU 할당
// CPU 를 빼앗을 수 없다
Queue Non_Preemptive_Priority(Process *job_queue, int n) {
    char gantt_chart1[200] = {0};
    char gantt_chart2[200] = {0};
    char gantt_line[200] = {0};

    int time = 0;
    int CPU_busy = 0;
    int IO_busy = 0;

    Queue ready_queue;
    Queue waiting_queue;
    Queue terminated_queue;
    config_queues(&ready_queue, &waiting_queue, &terminated_queue);

    Process running = {0};
    Process IO = {0};

    for (time = 0; terminated_queue.count < n; time++) {
        // IO terminate
        if (IO_busy && IO.io_burst_time <= 0) {
            push_queue(&ready_queue, IO);
            IO_busy = 0;
        }

        // job queue -> ready queue
        for (int i = 0; i < n; i++) {
            if (job_queue[i].arrival_time == time) {
                push_queue(&ready_queue, job_queue[i]);
            }
        }

        // IO terminate, job queue -> ready queue
        // priority 에 대해 오름차순으로 정렬
        sort_processes_priority(ready_queue.processes, ready_queue.count);


        // CPU running 프로세스 -> IO operation
        if (CPU_busy && running.io_burst_time != 0 && running.cpu_burst_time == running.io_start) {
            running.waiting_queue_time = time; 
            push_queue(&waiting_queue, running);
            CPU_busy = 0;
        }

        // ready queue -> CPU running
        if (!CPU_busy && ready_queue.count > 0) {
            running = pop_queue(&ready_queue);
            running.cpu_burst_time--;

            char str[10];
            sprintf(str, "====");
            strcat(gantt_line, str);
            sprintf(str, "|P%2d", running.pid);
            strcat(gantt_chart1, str);
            sprintf(str, "%-4d", time);
            strcat(gantt_chart2, str);

            CPU_busy = 1;
        } 
        
        // idle
        else if (!CPU_busy && ready_queue.count <= 0) {
            char str[10];
            sprintf(str, "=====");
            strcat(gantt_line, str);
            sprintf(str, "|idle");
            strcat(gantt_chart1, str);
            sprintf(str, "%-5d", time);
            strcat(gantt_chart2, str);
        } 
        
        // CPU running continue
        else {
            char str[10];
            sprintf(str, "====");
            strcat(gantt_line, str);
            sprintf(str, "|P%2d", running.pid);
            strcat(gantt_chart1, str);
            sprintf(str, "%-4d", time);
            strcat(gantt_chart2, str);

            running.cpu_burst_time--;
        }

        // terminate
        if (CPU_busy && running.cpu_burst_time <= 0) {
            CPU_busy = 0;
            running.terminate_time = time+1;
            push_queue(&terminated_queue, running);
        }

        // waiting queue -> IO
        if (!IO_busy && waiting_queue.count > 0) {
            IO = pop_queue(&waiting_queue);
            IO.waiting_queue_time = time - IO.waiting_queue_time; 
            IO.io_burst_time--;
            IO_busy = 1;
        } 
        
        // IO continue
        else if (IO_busy) {
            IO.io_burst_time--;
        }

    }

    char str[10];
    sprintf(str, "=");
    strcat(gantt_line, str);
    sprintf(str, "|");
    strcat(gantt_chart1, str);
    sprintf(str, "%-5d", time);
    strcat(gantt_chart2, str);
    printf("%s\n%s\n%s\n%s\n", gantt_line, gantt_chart1, gantt_line, gantt_chart2);

    return terminated_queue;
}

// preemptive priority
// priority가 높은(=숫자가 작은) 순으로 CPU 할당
// CPU 를 빼앗을 수 있다
Queue Preemptive_Priority(Process *job_queue, int n) {
    char gantt_chart1[200] = {0};
    char gantt_chart2[200] = {0};
    char gantt_line[200] = {0};

    int time = 0;
    int CPU_busy = 0;
    int IO_busy = 0;

    Queue ready_queue;
    Queue waiting_queue;
    Queue terminated_queue;
    config_queues(&ready_queue, &waiting_queue, &terminated_queue);

    Process running = {0};
    Process IO = {0};

    for (time = 0; terminated_queue.count < n; time++) {
        // IO terminate
        if (IO_busy && IO.io_burst_time <= 0) {
            push_queue(&ready_queue, IO);
            IO_busy = 0;
        }

        // job queue -> ready queue
        for (int i = 0; i < n; i++) {
            if (job_queue[i].arrival_time == time) {
                push_queue(&ready_queue, job_queue[i]);
            }
        }

        // IO terminate, job queue -> ready queue
        // priority 에 대해 오름차순으로 정렬
        sort_processes_priority(ready_queue.processes, ready_queue.count);

        // CPU running 프로세스 -> IO operation
        if (CPU_busy && running.io_burst_time != 0 && running.cpu_burst_time == running.io_start) {
            running.waiting_queue_time = time; 
            push_queue(&waiting_queue, running);
            CPU_busy = 0;
        }

        // running 프로세스가 교체되는 case 1 : ready queue -> running
        if (!CPU_busy && ready_queue.count > 0) {
            running = pop_queue(&ready_queue);
            CPU_busy = 1;
        } 


        // running 프로세스가 교체되는 case 2 : preemptive
        else if (CPU_busy && ready_queue.count > 0 && ready_queue.processes[0].priority < running.priority) {
            Process temp = running;
            running = pop_queue(&ready_queue);
            push_queue(&ready_queue, temp);
        }

        // CPU running continue
        if (CPU_busy) {
            running.cpu_burst_time--;

            char str[10];
            sprintf(str, "====");
            strcat(gantt_line, str);
            sprintf(str, "|P%2d", running.pid);
            strcat(gantt_chart1, str);
            sprintf(str, "%-4d", time);
            strcat(gantt_chart2, str);
        } 
        
        // idle
        else {
            char str[10];
            sprintf(str, "=====");
            strcat(gantt_line, str);
            sprintf(str, "|idle");
            strcat(gantt_chart1, str);
            sprintf(str, "%-5d", time);
            strcat(gantt_chart2, str);
        }

        // terminate
        if (CPU_busy && running.cpu_burst_time <= 0) {
            CPU_busy = 0;
            running.terminate_time = time+1;
            push_queue(&terminated_queue, running);
        }

        // waiting queue -> IO
        if (!IO_busy && waiting_queue.count > 0) {
            IO = pop_queue(&waiting_queue);
            IO.waiting_queue_time = time - IO.waiting_queue_time; 
            IO.io_burst_time--;
            IO_busy = 1;
        } 
        
        // IO continue
        else if (IO_busy) {
            IO.io_burst_time--;
        }
    }

    char str[10];
    sprintf(str, "=");
    strcat(gantt_line, str);
    sprintf(str, "|");
    strcat(gantt_chart1, str);
    sprintf(str, "%-5d", time);
    strcat(gantt_chart2, str);
    printf("%s\n%s\n%s\n%s\n", gantt_line, gantt_chart1, gantt_line, gantt_chart2);

    return terminated_queue;
}

// round robin
// TIME QUANTUM 을 초과하면 CPU를 재할당한다
Queue Round_Robin(Process *job_queue, int n) {
    char gantt_chart1[200] = {0};
    char gantt_chart2[200] = {0};
    char gantt_line[200] = {0};

    int time = 0;
    int CPU_busy = 0;
    int IO_busy = 0;
    int time_quantum_remaining = TIME_QUANTUM;  // 남은 time quantum을 기록한다

    Queue ready_queue;
    Queue waiting_queue;
    Queue terminated_queue;
    config_queues(&ready_queue, &waiting_queue, &terminated_queue);

    Process running = {0};
    Process IO = {0};

    for (time = 0; terminated_queue.count < n; time++) {
        // IO terminate
        if (IO_busy && IO.io_burst_time <= 0) {
            push_queue(&ready_queue, IO);
            IO_busy = 0;
        }

        // job queue -> ready queue
        for (int i = 0; i < n; i++) {
            if (job_queue[i].arrival_time == time) {
                push_queue(&ready_queue, job_queue[i]);
            }
        }

        // 시간 퀀텀을 모두 사용한 경우 running -> ready queue
        if (CPU_busy && time_quantum_remaining <= 0) {
            push_queue(&ready_queue, running);
            CPU_busy = 0;
            time_quantum_remaining = TIME_QUANTUM;
        }


        // CPU running 프로세스 -> IO operation
        if (CPU_busy && running.io_burst_time != 0 && running.cpu_burst_time == running.io_start) {
            running.waiting_queue_time = time;
            push_queue(&waiting_queue, running);
            CPU_busy = 0;
            time_quantum_remaining = TIME_QUANTUM;  // 새로운 running 프로세스 할당 위해 시간 퀀텀 초기화
        }

        // ready queue -> CPU running
        if (!CPU_busy && ready_queue.count > 0) {
            running = pop_queue(&ready_queue);
            CPU_busy = 1;
            time_quantum_remaining = TIME_QUANTUM;  // 새로운 running 프로세스 할당 위해 시간 퀀텀 초기화
        }

        // CPU running continue
        if (CPU_busy) {
            running.cpu_burst_time--;
            time_quantum_remaining--;   // time quantum 을 감소시킨다

            char str[10];
            sprintf(str, "====");
            strcat(gantt_line, str);
            sprintf(str, "|P%2d", running.pid);
            strcat(gantt_chart1, str);
            sprintf(str, "%-4d", time);
            strcat(gantt_chart2, str);
        } 
        
        // idle
        else {
            char str[10];
            sprintf(str, "=====");
            strcat(gantt_line, str);
            sprintf(str, "|idle");
            strcat(gantt_chart1, str);
            sprintf(str, "%-5d", time);
            strcat(gantt_chart2, str);
        }

        // terminate
        if (CPU_busy && running.cpu_burst_time <= 0) {
            CPU_busy = 0;
            running.terminate_time = time+1;
            push_queue(&terminated_queue, running);
            time_quantum_remaining = TIME_QUANTUM;  // 새로운 running 프로세스 할당 위해 시간 퀀텀 초기화
        }

        // waiting queue -> IO
        if (!IO_busy && waiting_queue.count > 0) {
            IO = pop_queue(&waiting_queue);
            IO.waiting_queue_time = time - IO.waiting_queue_time;
            IO.io_burst_time--;
            IO_busy = 1;
        } 
        
        // IO continue
        else if (IO_busy) {
            IO.io_burst_time--;
        }

    }

    char str[10];
    sprintf(str, "=");
    strcat(gantt_line, str);
    sprintf(str, "|");
    strcat(gantt_chart1, str);
    sprintf(str, "%-5d", time);
    strcat(gantt_chart2, str);
    printf("%s\n%s\n%s\n%s\n", gantt_line, gantt_chart1, gantt_line, gantt_chart2);

    return terminated_queue;
}

void print_process(Process* processes, int n) {
    puts("==================================== Process ==================================");
    puts("+-------+--------------+----------+----------------+----------+---------------+");
    puts("|  PID  | Arrival Time | Priority | CPU Burst Time | IO Start | IO Burst Time |");
    puts("+-------+--------------+----------+----------------+----------+---------------+");

    for (int i = 0; i < n; i++) {
        printf("|   %d   |      %2d      |    %2d    |       %2d       |    %2d    |      %2d       |\n",
               processes[i].pid, processes[i].arrival_time, processes[i].priority,
               processes[i].cpu_burst_time, processes[i].cpu_burst_time - processes[i].io_start, processes[i].io_burst_time);
        puts("+-------+--------------+----------+----------------+----------+---------------+");
    }
    puts("===============================================================================");
    puts("\n");
}

// evaluate - waiting time, turnaround time
void calculate_times(Process *original, Process *terminated, int n) {
    double total_waiting_time = 0;
    double total_turnaround_time = 0;
    int pid = 0;

    for (int i = 0; i < n; i++) {

        pid = terminated[i].pid;

        // turnaround time = terminate time - arrival time
        terminated[i].turnaround_time = terminated[i].terminate_time - original[pid].arrival_time;
        // waiting time = turnaround time - cpu burst time - io burst time - waiting queue time
        // waiting queue time : waiting queue에서 대기하는 시간은 waiting time에서 제외
        // waiting time에는 ready queue에서 대기하는 시간만 포함
        terminated[i].waiting_time = terminated[i].turnaround_time - original[pid].cpu_burst_time - original[pid].io_burst_time - terminated[i].waiting_queue_time;
    }

    for (int i = 0; i < n; i++) {
        total_waiting_time += terminated[i].waiting_time;
        total_turnaround_time += terminated[i].turnaround_time;
    }

    printf("Average Waiting Time: %.2f\n", total_waiting_time / n);
    printf("Average Turnaround Time: %.2f\n", total_turnaround_time / n);
}

void print_case(void) {
    puts("--------------CPU Scheduling Algorithms--------------");
    puts("1 : FCFS");
    puts("2 : Non-Preemptive SJF");
    puts("3 : Preemptive SJF");
    puts("4 : Non-Preemptive Priority");
    puts("5 : Preemptive Priority");
    puts("6 : Round Robin");
    puts("7 : Exit");
    printf("Choice > ");
    return;
}



int main(void) {
    Process processes[PROCESS_NUM] = {0};
    Process job_queue[PROCESS_NUM] = {0};
    Queue terminated;

    create_processes(processes, PROCESS_NUM);
    sort_processes_pid(processes, PROCESS_NUM);
    print_process(processes, PROCESS_NUM);

    int choice = 0;

    while(1) {
        print_case();
        scanf("%d", &choice);

        switch (choice) {
            case 1 :
                puts("\n\n-----FCFS Gantt Chart-----");
                memcpy(job_queue, processes, sizeof(processes));
                terminated = FCFS(job_queue, PROCESS_NUM);
                puts("\n------FCFS Evaluation-----");
                calculate_times(processes, terminated.processes, PROCESS_NUM);
                puts("--------------------------\n\n");
                break;

            case 2 : 
                puts("\n\n-----Non Preemptive SJF Gantt Chart-----");
                memcpy(job_queue, processes, sizeof(processes));
                terminated = Non_Preemptive_SJF(job_queue, PROCESS_NUM);
                puts("\n-----Non Preemptive SJF Evaluation------");
                calculate_times(processes, terminated.processes, PROCESS_NUM);
                puts("----------------------------------------\n\n");
                break;

            case 3 :
                puts("\n\n-----Preemptive SJF Gantt Chart-----");
                memcpy(job_queue, processes, sizeof(processes));
                terminated = Preemptive_SJF(job_queue, PROCESS_NUM);
                puts("\n-----Preemptive SJF Evaluation------");
                calculate_times(processes, terminated.processes, PROCESS_NUM);
                puts("------------------------------------\n\n");
                break;

            case 4 :
                puts("\n\n-----Non Preemptive Priority Gantt Chart-----");
                memcpy(job_queue, processes, sizeof(processes));
                terminated = Non_Preemptive_Priority(job_queue, PROCESS_NUM);
                puts("\n-----Non Preemptive Priority Evaluation------");
                calculate_times(processes, terminated.processes, PROCESS_NUM);
                puts("---------------------------------------------\n\n");
                break;

            case 5 :
                puts("\n\n-----Preemptive Priority Gantt Chart-----");
                memcpy(job_queue, processes, sizeof(processes));
                terminated = Preemptive_Priority(job_queue, PROCESS_NUM);
                puts("\n-----Preemptive Priority Evaluation------");
                calculate_times(processes, terminated.processes, PROCESS_NUM);
                puts("-----------------------------------------\n\n");
                break;

            case 6 :
                puts("\n\n-----Round Robin Gantt Chart-----");
                memcpy(job_queue, processes, sizeof(processes));
                terminated = Round_Robin(job_queue, PROCESS_NUM);
                puts("\n-----Round Robin Evaluation------");
                calculate_times(processes, terminated.processes, PROCESS_NUM);
                puts("---------------------------------\n\n");
                break;

            default :
                puts("Exit");
                exit(0);
        }
    }

    return 0;
}
