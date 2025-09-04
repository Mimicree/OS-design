#include "./types.h"
#include "./multitasking.h"
#include "./io.h"

// An array to hold all of the processes we create
proc_t processes[MAX_PROCS];

// Keep track of the next index to place a newly created process in the process array
uint8 process_index = 0;

proc_t *prev;       // The previously ran user process
proc_t *running;    // The currently running process, can be either kernel or user process
proc_t *next;       // The next process to run
proc_t *kernel;     // The kernel process

// Select the next user process (proc_t *next) to run
// Selection must be made from the processes array (proc_t processes[])
int schedule(){ //little did i know my previous yield function would cost me significant time troubleshooting here. 
                //(yield called schedule twice which caused all kinds of weird issues)
    static int previousProcess = 0; //static makes this persist across calls

    for (int i = 1; i <= 5; i++){
        int processID = (previousProcess + i) % 6; //start at 1 after previous process pid and start checking from there up to 5 then start over.

            proc_t *isReady = &processes[processID];
            
            if (isReady->type == PROC_TYPE_USER && isReady->status == PROC_STATUS_READY){
                next = isReady; 
                previousProcess = processID;
                return 1; //process found to run
            }
    }

    return 0;
}

int ready_process_count()
{
    int count = 0;

    for (int i = 0; i < MAX_PROCS; i++)
    {
        proc_t *current = &processes[i];

        if (current->type == PROC_TYPE_USER && current->status == PROC_STATUS_READY)
        {
            count++;
        }
    }

    return count;
}


// Create a new user process
// When the process is eventually ran, start executing from the function provided (void *func)
// Initialize the stack top and base at location (void *stack)
// If we have hit the limit for maximum processes, return -1
// Store the newly created process inside the processes array (proc_t processes[])
int createproc(void *func, char *stack)
{
    if(process_index > MAX_USER_PROCS){
        return -1; //prevents too many from being created
    }

    else {
        proc_t *newProc = &processes[process_index]; //had some trouble with understanding these. not used to the syntax
        newProc->status = PROC_STATUS_READY;  //mainly followed the start kernel function to get it to work
        newProc->type = PROC_TYPE_USER;
        newProc->ebp = stack; //think this is the proper way to start a stack in this?
        newProc->esp = stack;
        newProc->eip = func; //start instruction here
        newProc->pid = process_index;
        process_index ++;
    

        return 0;
    }
    
}

// Create a new kernel process
// The kernel process is ran immediately, executing from the function provided (void *func)
// Stack does not to be initialized because it was already initialized when main() was called
// If we have hit the limit for maximum processes, return -1
// Store the newly created process inside the processes array (proc_t processes[])
int startkernel(void func())
{
    // If we have filled our process array, return -1
    if(process_index >= MAX_PROCS)
    {
        return -1;
    }

    // Create the new kernel process
    proc_t kernproc;
    kernproc.status = PROC_STATUS_RUNNING; // Processes start ready to run
    kernproc.type = PROC_TYPE_KERNEL;    // Process is a kernel process

    // Assign a process ID and add process to process array
    kernproc.pid = process_index;
    processes[process_index] = kernproc;
    kernel = &processes[process_index];     // Use a proc_t pointer to keep track of the kernel process so we don't have to loop through the entire process array to find it
    process_index++;

    // Assign the kernel to the running process and execute
    running = kernel;
    func();

    return 0;
}

// Terminate the process that is currently running (proc_t current)
// Assign the kernel as the next process to run
// Context switch to the kernel process
void exit()
{
    
    if(running->type == PROC_TYPE_USER){ //iff a user proc then terminate and set next to kernel and switch, else return
        running->status = PROC_STATUS_TERMINATED;
        next = kernel;
        contextswitch();
    }

    return;
}

// Yield the current process
// This will give another process a chance to run
// If we yielded a user process, switch to the kernel process
// If we yielded a kernel process, switch to the next process
// The next process should have already been selected via scheduling
void yield()
{
    
    running->status = PROC_STATUS_READY;
   // int scheduleSuccess = 0;

    if(running->type == PROC_TYPE_KERNEL){ //iff kernel proc run schedule, i dont think this runs sched twice but it wouldnt
      //scheduleSuccess = schedule();        //work without both. ***this caused so much unknown headache in project 4.***
      schedule();

      //  if (!scheduleSuccess){ // i tried to add a safeguard in case the schedule fails, not sure if needed or not.
      //      next = kernel;     //(there may be a better way to check if sched failed or not i just dont know how)
       // }
    }
    
    else{ //sets kernel next for any user procs
        next = kernel;
    }

    contextswitch(); //pulled switch out of if statement because it seemed redundant to have twice. I struggled a little
}                    // working with and understanding how the context switch function was executing.

// Performs a context switch, switching from "running" to "next"
void contextswitch()
{
    // In order to perform a context switch, we need perform a system call
    // The system call takes inputs via registers, in this case eax, ebx, and ecx
    // eax = system call code (0x01 for context switch)
    // ebx = the address of the process control block for the currently running process
    // ecx = the address of the process control block for the process we want to run next

    // Save registers for later and load registers with arguments
    asm volatile("push %eax");
    asm volatile("push %ebx");
    asm volatile("push %ecx");
    asm volatile("mov %0, %%ebx" : :    "r"(&running));
    asm volatile("mov %0, %%ecx" : :    "r"(&next));
    asm volatile("mov $1, %eax");

    // Call the system call
    asm volatile("int $0x80");

    // Pop the previously pushed registers when we return eventually
    asm volatile("pop %ecx");
    asm volatile("pop %ebx");
    asm volatile("pop %eax");
}
