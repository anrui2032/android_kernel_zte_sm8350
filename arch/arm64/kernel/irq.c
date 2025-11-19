// SPDX-License-Identifier: GPL-2.0-only
/*
 * Based on arch/arm/kernel/irq.c
 *
 * Copyright (C) 1992 Linus Torvalds
 * Modifications for ARM processor Copyright (C) 1995-2000 Russell King.
 * Support for Dynamic Tick Timer Copyright (C) 2004-2005 Nokia Corporation.
 * Dynamic Tick Timer written by Tony Lindgren <tony@atomide.com> and
 * Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>.
 * Copyright (C) 2012 ARM Ltd.
 */

#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/irqchip.h>
#include <linux/kprobes.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include <asm/daifflags.h>
#include <asm/vmap_stack.h>
#include <asm/scs.h>

unsigned long irq_err_count;

/* Only access this in an NMI enter/exit */
DEFINE_PER_CPU(struct nmi_ctx, nmi_contexts);

DEFINE_PER_CPU(unsigned long *, irq_stack_ptr);

int arch_show_interrupts(struct seq_file *p, int prec)
{
	show_ipi_list(p, prec);
	seq_printf(p, "%*s: %10lu\n", prec, "Err", irq_err_count);
	return 0;
}

/* zte_pm - begin */
void print_irq_info(int i)
{
	struct irqaction *action;
	struct irq_desc *zte_irq_desc;
	unsigned long flags;
	int wake_prop = 0;

	zte_irq_desc = irq_to_desc(i);
	if (zte_irq_desc) {
		raw_spin_lock_irqsave(&zte_irq_desc->lock, flags);
		action = zte_irq_desc->action;
		/* zte_pm wake_prop>0 u can print wake_depth */
		wake_prop = zte_irq_desc->wake_depth;
		if (!action)
			goto unlock;

		pr_info("[IRQ] num=%d, chipName=%10s, actionName=%s", i,
						zte_irq_desc->irq_data.chip->name ? : "-",
						action->name);
		if (wake_prop > 0)
			pr_info("[IRQ] wake_depth=%d", wake_prop);

		for (action = action->next; action; action = action->next)
			pr_info("[IRQ] show all action->name=%s\n", action->name);

unlock:
		raw_spin_unlock_irqrestore(&zte_irq_desc->lock, flags);
	} else {
		pr_err("[IRQ] error in dump irq info for irq %d\n", i);
	}
}
/* zte_pm - end */

#ifdef CONFIG_VMAP_STACK
static void init_irq_stacks(void)
{
	int cpu;
	unsigned long *p;

	for_each_possible_cpu(cpu) {
		p = arch_alloc_vmap_stack(IRQ_STACK_SIZE, cpu_to_node(cpu));
		per_cpu(irq_stack_ptr, cpu) = p;
	}
}
#else
/* irq stack only needs to be 16 byte aligned - not IRQ_STACK_SIZE aligned. */
DEFINE_PER_CPU_ALIGNED(unsigned long [IRQ_STACK_SIZE/sizeof(long)], irq_stack);

static void init_irq_stacks(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
		per_cpu(irq_stack_ptr, cpu) = per_cpu(irq_stack, cpu);
}
#endif

void __init init_IRQ(void)
{
	init_irq_stacks();
	scs_init_irq();
	irqchip_init();
	if (!handle_arch_irq)
		panic("No interrupt controller found.");

	if (system_uses_irq_prio_masking()) {
		/*
		 * Now that we have a stack for our IRQ handler, set
		 * the PMR/PSR pair to a consistent state.
		 */
		WARN_ON(read_sysreg(daif) & PSR_A_BIT);
		local_daif_restore(DAIF_PROCCTX_NOIRQ);
	}
}

/*
 * Stubs to make nmi_enter/exit() code callable from ASM
 */
asmlinkage void notrace asm_nmi_enter(void)
{
	nmi_enter();
}
NOKPROBE_SYMBOL(asm_nmi_enter);

asmlinkage void notrace asm_nmi_exit(void)
{
	nmi_exit();
}
NOKPROBE_SYMBOL(asm_nmi_exit);
