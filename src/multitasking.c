#include <multitasking.h>
#include <common/types.h>
#include <gdtdesc.h>
#include <io/screen.h>
#define MAX_TASKS 256
#define KSTACK_SIZE 4096

/// @brief creates a new task
/// @param callback a pointer to the function the task needs to exec
/// @return the task created
task_t create_task(uint32_t callback, uint32_t user_stack, uint32_t kernel_stack, bool is_kernel_task)
{
    task_t task;
    

	if (!is_kernel_task) {
		// we can pass things to the task by pushing to its user stack
		// with cdecl, this will pass it as arguments
		user_stack -= 4;
		*(uint32_t*) user_stack = 0; // task func return address, shouldnt be used
	}

    uint32_t cs = is_kernel_task ? KERNEL_CS : (USER_CS | 3);
    uint32_t ds = is_kernel_task ? KERNEL_DS : (USER_DS | 3);

    uint8_t* kesp = (uint8_t*) kernel_stack;
    
    kesp -= sizeof(NewTaskInitStack);
    NewTaskInitStack* init_stack = (NewTaskInitStack*) kesp;
    init_stack -> ebp = init_stack -> edi = init_stack -> esi = init_stack -> ebx = 0;
    init_stack -> return_addr = (uint32_t) new_task_setup;
    init_stack -> data_selector = ds;
    init_stack -> eip = callback;
    init_stack -> cs = cs;
    init_stack -> eflags = 0x200;

    init_stack -> useresp = user_stack;
    init_stack -> ss = ds;

    task.kstack = (uint32_t) kesp;
    task.kstack_bottom = kernel_stack;
    return task;
}

//initial taskManager values
int numTasks = 0;
int currentTask = -1;
task_t* tasks[MAX_TASKS]; 

/// @brief adds a task to tasks loop
/// @param task the task to be added
/// @return true if successfully added, otherwise, false
bool add_task(task_t* task)
{
    if(numTasks >= MAX_TASKS)
        return false;
    tasks[numTasks++] = task;
    return true;
}

/// @brief Saves cpu state of current task and returns the state of the next task
/// @param cpuState the current cpu state 
/// @return the xpu state of the next task
void schedule()
{
    if(numTasks <= 0)
        return;
    
    task_t* old;
    task_t* next;

    if(currentTask >= 0)
       old = tasks[currentTask];
    
    if(++currentTask >= numTasks)
        currentTask %= numTasks;
    
    next = tasks[currentTask];
    change_tss_esp0(next->kstack_bottom);
    switch_context(old, next);
}