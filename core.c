// SPDX-License-Identifier: GPL-2.0
/**
 * Performance events core code:
 *
 *  Copyright (C) 2008 Thomas Gleixner <tglx@linutronix.de>
 *  Copyright (C) 2008-2011 Red Hat, Inc., Ingo Molnar
 *  Copyright (C) 2008-2011 Red Hat, Inc., Peter Zijlstra
 *  Copyright (C) 2009 Paul Mackerras, IBM Corp. <paulus@au1.ibm.com>
 */

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/idr.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/hash.h>
#include <linux/tick.h>
#include <linux/sysfs.h>
#include <linux/dcache.h>
#include <linux/percpu.h>
#include <linux/ptrace.h>
#include <linux/reboot.h>
#include <linux/vmstat.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/vmalloc.h>
#include <linux/hardirq.h>
#include <linux/hugetlb.h>
#include <linux/rculist.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/anon_inodes.h>
#include <linux/kernel_stat.h>
#include <linux/cgroup.h>
#include <linux/perf_event.h>
#include <linux/trace_events.h>
#include <linux/hw_breakpoint.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/mman.h>
#include <linux/compat.h>
#include <linux/bpf.h>
#include <linux/filter.h>
#include <linux/namei.h>
#include <linux/parser.h>
#include <linux/sched/clock.h>
#include <linux/sched/mm.h>
#include <linux/proc_ns.h>
#include <linux/mount.h>
#include <linux/min_heap.h>

#include "internal.h"

#include <asm/irq_regs.h>

typedef int (*remote_function_f)(void *);

struct remote_function_call
{
  struct task_struct *p;
  remote_function_f func;
  void *info;
  int ret;
}

static void
remote_function(void *data)
{
  struct remote_function_call *tfc = data;
  struct task_struct *p = tfc->p;

  if (p)
  {
    /* -EAGAIN */
    if (task_cpu(p) != smp_processor_id())
      return;

    /*
     * Now that we're on right CPU with IRQs disabled, we can test
     * if we hit the right task without races.
     */

    tfc->ret = -ESRCH; /* No such (running) process */

    if (p != current)
      return;
  }

  tfc->ret = tfc->func(tfc->info);
}

/**
 * task_function_call - call a function on the cpu on which a task runs
 * @p: the task to evaluate
 * @func: the function to be called
 * @info: the function call argument
 *
 * Calls the function @func when the task is currently running. This might
 * be on the curren CPU, which just calls the function directly, This will
 * retry due to any failures in smp_call_function_single(), such as if the
 * task_cpu() goes offline concurrently.
 *
 * returns @func return value or -ESRCH or -ENXIO when the process isn't running
 */
static int
task_function_call(struct task_struct *p, remote_function_f func, void *info)
{
  struct remote_function_call data = {
      .p = p,
      .func = func,
      .info = info,
      .ret = -EAGAIN,
  };
  int ret;

  for (;;)
  {
    ret = smp_call_function_single(task_cpu(p), remote_function, &data, 1);

    if (!ret)
      ret = data.ret;

    if (ret != -EAGAIN)
      break;

    cond_resched();
  }

  return ret;
}

/**
 * @remarks
 * 여기서부터 작성 예정
 */