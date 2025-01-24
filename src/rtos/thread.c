
#include "rtos/thread.h"

#include "hal/stm32f4_blackpill.h"
#include "utils/dbc_assert.h"

#include <stddef.h>
#include <stdint.h>

/* Interrupt Priority Registers */
#define NVIC_SHPR2_REG (*((volatile uint32_t *)0xE000ED1C))
#define NVIC_SHPR3_REV (*((volatile uint32_t *)0xE000ED20))

#define RTOS_IDLE_THREAD_PRIORITY (0U)

#ifndef __ARM_FEATURE_CLZ
#error "CLZ instruction not supported!"
#endif

/* Use Log2 to identify the index of the highest priority thread by counting the
 * leading zeros of the bitfield */
#define LOG2(x) (32 - __builtin_clz((x)))

/* Pointer to current/next thread for scheduling */
rtos_thread_t *volatile rtos_current;
rtos_thread_t *volatile rtos_next;

rtos_thread_t *rtos_threads[32 + 1] = {NULL};
uint32_t rtos_ready_set = 0;
uint32_t rtos_delayed_set = 0;

rtos_thread_t idle_thread = {0};
void idle_thread_handler()
{
    for (;;) {
        rtos_on_idle();
    }
}

void rtos_init(void *const idle_thread_stack, uint32_t const idle_thread_stack_size)
{
    /* Set PendSV Interrupt to have lowest Priority
     * Equivilent of `NVIC_setPriority(PendSV_IRQn, 0xFFU)` */
    NVIC_SHPR3_REV |= (0xFFU << 16U);

    /* create idle thread */
    rtos_thread_create(
        &idle_thread,
        idle_thread_handler,
        idle_thread_stack,
        idle_thread_stack_size,
        RTOS_IDLE_THREAD_PRIORITY);
}

void rtos_schedule(void)
{
    /* No threads are ready to run, so set to Idle thread */
    if (rtos_ready_set == 0U) {
        rtos_next = rtos_threads[0]; /* idle thread */
    } else {
        /* Find next thread to run */
        rtos_next = rtos_threads[LOG2(rtos_ready_set)];
        DBC_ASSERT(rtos_next != NULL);
    }

    if (rtos_next != rtos_current) {
        /* raise pendsv irq by setting "set pending" bit of interrupt and control state register
         * (ICSR) */
        *(volatile uint32_t *)0xE000ED04 = (1U << 28U);
    }
}
void rtos_run(void)
{
    rtos_on_startup();

    disable_irq();
    rtos_schedule();
    enable_irq();

    /* the following should never execute */
    DBC_ERROR();
}

void rtos_tick(void)
{
    uint32_t working_set = rtos_delayed_set;
    while (working_set != 0U) {
        rtos_thread_t *thread = rtos_threads[LOG2(working_set)];
        DBC_ASSERT(thread != NULL);
        DBC_ASSERT(thread->timeout != 0U);

        uint32_t thread_bit = (1U << (thread->priority - 1U));
        --thread->timeout;
        if (thread->timeout == 0U) {
            rtos_ready_set |= thread_bit;
            rtos_delayed_set &= ~thread_bit;
        }
        working_set &= ~thread_bit;
    }
}

void rtos_delay(uint32_t ticks)
{
    disable_irq();

    /* never call rtos_delay from the idle thread */
    DBC_REQUIRE(rtos_current != rtos_threads[0]);

    uint32_t thread_bit = (1U << (rtos_current->priority - 1U));
    rtos_current->timeout = ticks;
    rtos_ready_set &= ~thread_bit;
    rtos_delayed_set |= thread_bit;
    rtos_schedule();

    enable_irq();
}

void rtos_thread_create(
    rtos_thread_t *const self,
    rtos_thread_handler_t const handler,
    void *const stack_base,
    uint32_t const stack_size,
    uint8_t const priority)
{
    DBC_REQUIRE(priority < (sizeof(rtos_threads) / sizeof(rtos_threads[0U])));
    DBC_REQUIRE(rtos_threads[priority] == NULL);

    /* get stack pointer and ensure aligned at the 8 byte boudary */
    uint32_t *sp = (uint32_t *)((((uint32_t)stack_base + stack_size) / 8U) * 8U);

    /* Setup stack according to Arm procedure call standard */
    *(--sp) = (1U << 24); /* xPSR, set 24'th bit for thumb mode (invalid if not set on cortex-m
                             chips) */
    *(--sp) = (uint32_t)handler; /* PC (Program Counter) */
    *(--sp) = 0x0000000EU;       /* LR (Link Register) */
    *(--sp) = 0x0000000CU;       /* R12 */
    *(--sp) = 0x00000003U;       /* R3 */
    *(--sp) = 0x00000002U;       /* R2 */
    *(--sp) = 0x00000001U;       /* R1 */
    *(--sp) = 0x00000000U;       /* R0 */

    /* Initialise Additional Registers on Stack */
    *(--sp) = 0x0000000BU; /* R11 */
    *(--sp) = 0x0000000AU; /* R10 */
    *(--sp) = 0x00000009U; /* R9 */
    *(--sp) = 0x00000008U; /* R8 */
    *(--sp) = 0x00000007U; /* R7 */
    *(--sp) = 0x00000006U; /* R6 */
    *(--sp) = 0x00000005U; /* R5 */
    *(--sp) = 0x00000004U; /* R4 */

    /* Save current stack pointer into self */
    self->sp = sp;

    /* Get aligned bottom of stack */
    uint32_t *stack_limit = (uint32_t *)(((((uint32_t)stack_base - 1U) / 8U) + 1U) * 8U);

    /* pre-fill rest of stack with stack paint */
    for (sp = (sp - 1U); sp >= stack_limit; --sp) {
        *sp = 0xBABECAFE;
    }


    /* Register thread with OS */
    rtos_threads[priority] = self;
    self->priority = priority;
    /* Mark thread as ready to run */
    if (priority > 0U) {
        rtos_ready_set |= (1U << (priority - 1U));
    }
}

void PendSV_Handler(void)
{
    asm volatile(
        /* disable interrupts */
        "    cpsid i\n\t"
        /* if {rtos_current != (rtos_thread_t *)0U) { */
        "    ldr   r1,=rtos_current\n\t"
        "    ldr   r1,[r1,#0]\n\t"
        "    cbz   r1,PendSV_restore\n\t"
        /* push r4-r11 onto the stack */
        "    push  {r4-r11}\n\t"
        "    ldr   r1,=rtos_current\n\t"
        "    ldr   r1,[r1,#0]\n\t"
        /* rtos_current->sp = sp; */
        "    str   sp,[r1,#0]\n\t"
        /* } */

        "PendSV_restore:\n\t"
        /* sp = rtos_next->sp; */
        "    ldr   r1,=rtos_next\n\t"
        "    ldr   r1,[r1,#0]\n\t"
        "    ldr   sp,[r1,#0]\n\t"
        /* rtos_current = rtos_next; */
        "    ldr   r1,=rtos_next\n\t"
        "    ldr   r1,[r1,#0]\n\t"
        "    ldr   r2,=rtos_current\n\t"
        "    str   r1,[r2,#0]\n\t"
        /* pop registers r4-r11 */
        "    pop   {r4-r11}\n\t"
        "    cpsie i\n\t"
        "    bx    lr\n\t");

#if 0
    void *sp = NULL;

    __enable_irq();
    if (rtos_thread_current != (rtos_thread_t *)0U) {
        /* Push registers r4-r11 onto the stack */
        rtos_thread_current->sp = sp;
    }
    sp = rtos_thread_next->sp;
    rtos_thread_current = rtos_thread_next;
    /* Pop registers r4-r11 from stack */
    __disable_irq();
#endif
}
