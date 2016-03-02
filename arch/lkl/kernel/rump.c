/*
 * Rump hypercall interface for Linux
 * Copyright (c) 2015 Hajime Tazaki
 *
 * Author: Hajime Tazaki <thehajime@gmail.com>
 */

#include <linux/sched.h>
#include <asm/types.h>
#include <asm/unistd.h>
#include <asm/host_ops.h>
#include <asm/syscalls.h>

#include "rump.h"

static struct lwp *rump_libos_lwproc_curlwp(void);
static int rump_libos_lwproc_newlwp(pid_t pid);
static void rump_libos_lwproc_switch(struct lwp *newlwp);
static void rump_libos_lwproc_release(void);
static int rump_libos_lwproc_rfork(void *priv, int flags, const char *comm);

void
rump_schedule(void)
{
}

void
rump_unschedule(void)
{
}

int
rump_daemonize_begin(void)
{
	return 0;
}

int
rump_daemonize_done(int error)
{
	return 0;
}

#define RUMP_TEMP_STUB
#ifdef RUMP_TEMP_STUB
enum rump_etfs_type {
	RUMP_ETFS_REG,
	RUMP_ETFS_BLK,
	RUMP_ETFS_CHR,
	RUMP_ETFS_DIR,
	RUMP_ETFS_DIR_SUBDIRS
};

void rump_boot_setsigmodel(int rump_sigmodel){}
int rump_pub_etfs_register(const char *key, const char *hostpath, enum rump_etfs_type ftype){return 0;}
int rump_pub_etfs_register_withsize(const char *key, const char *hostpath,
	enum rump_etfs_type ftype, uint64_t begin, uint64_t size) {return 0;}
int rump___sysimpl_mount50(const char *str, const char *str2, int i, void *p, size_t s){return 0;}
int rump___sysimpl_dup2(int i, int j){return 0;}
int rump___sysimpl_socket30(int i, int j, int k){return 0;}
int rump___sysimpl_unmount(const char *str, int i){return 0;}
void __assert13(const char *file, int line, const char *function, const char *failedexpr){}
int rump___sysimpl_close(int fd) {return -1;}
int rump___sysimpl_ioctl(int fd, u_long com, void * data) {return -1;}
int rump___sysimpl_mkdir(const char * path, mode_t mode) {return -1;}
int rump___sysimpl_open(const char *name, int flags, ...) {return -1;}

#endif /* RUMP_TEMP_STUB */

int
rump_pub_lwproc_rfork(int arg1)
{
	int rv = 0;

	rump_schedule();
//	rv = rump_libos_lwproc_rfork(arg1);
	rump_unschedule();

	return rv;
}

int
rump_pub_lwproc_newlwp(pid_t arg1)
{
	int rv;

	rump_schedule();
	rv = rump_libos_lwproc_newlwp(arg1);
	rump_unschedule();

	return rv;
}

void
rump_pub_lwproc_switch(struct lwp *arg1)
{

	rump_schedule();
	rump_libos_lwproc_switch(arg1);
	rump_unschedule();
}

void
rump_pub_lwproc_releaselwp(void)
{

	rump_schedule();
	rump_libos_lwproc_release();
	rump_unschedule();
}

struct lwp *
rump_pub_lwproc_curlwp(void)
{
	struct lwp * rv;

	rump_schedule();
	rv = rump_libos_lwproc_curlwp();
	rump_unschedule();

	return rv;
}

int
rump_syscall(int num, void *data, size_t dlen, long *retval)
{
	int ret = 0;

	ret = lkl_syscall(num, (long *)data);
	/* FIXME: need better err translation */
	if (ret < 0) {
		retval[0] = -ret;
		ret = -1;
	}
	return ret;
}


static int
rump_libos_hyp_syscall(int num, void *arg, long *retval)
{
	return rump_syscall(num, arg, 0, retval);
}

static int
rump_libos_lwproc_rfork(void *priv, int flags, const char *comm)
{
	/* FIXME: needs new task_struct instead of get_current() */
	struct thread_info *ti = task_thread_info(get_current());

	/* store struct spc_client */
	ti->rump_client = priv;

	rumpuser_curlwpop(RUMPUSER_LWP_CREATE, (struct lwp *)ti);
	rumpuser_curlwpop(RUMPUSER_LWP_SET, (struct lwp *)ti);

	return 0;
}

static void
rump_libos_lwproc_release(void)
{
	struct thread_info *ti = (struct thread_info *)rumpuser_curlwp();

	rumpuser_curlwpop(RUMPUSER_LWP_CLEAR, (struct lwp *)ti);
}

static void
rump_libos_lwproc_switch(struct lwp *newlwp)
{
	struct thread_info *ti = (struct thread_info *)rumpuser_curlwp();

	rumpuser_curlwpop(RUMPUSER_LWP_CLEAR, (struct lwp *)ti);
	rumpuser_curlwpop(RUMPUSER_LWP_SET, (struct lwp *)ti);
}

/* find rump_task created by rfork */
static int
rump_libos_lwproc_newlwp(pid_t pid)
{
	/* find rump_task */
	struct thread_info *ti = NULL;
	struct task_struct *p;

	for_each_process(p) {
		if (p->pid == pid) {
			ti = task_thread_info(p);
			break;
		}
	}

	if (!ti) {
		pr_warn("newlwp: could not find pid %d\n", pid);
		ti = current_thread_info();
		/* FIXME */
//		return ESRCH;
	}

	/* set to currnet */
	rumpuser_curlwpop(RUMPUSER_LWP_SET, (struct lwp *)ti);

	return 0;
}

static struct lwp *
rump_libos_lwproc_curlwp(void)
{
	return rumpuser_curlwp();
}

static void
rump_libos_hyp_lwpexit(void)
{
	struct thread_info *ti = (struct thread_info *)rumpuser_curlwp();

	rumpuser_curlwpop(RUMPUSER_LWP_DESTROY, (struct lwp *)ti);
	free_thread_info(ti);
}

static pid_t
rump_libos_hyp_getpid(void)
{
	struct thread_info *ti = (struct thread_info *)rumpuser_curlwp();

	return ti->task->pid;
}


static void rump_libos_user_unschedule(int nlocks, int *countp,
				       void *interlock) {}
static void rump_libos_user_schedule(int nlocks, void *interlock) {}
static void rump_libos_hyp_execnotify(const char *comm) {}

const struct rumpuser_hyperup hyp = {
	.hyp_schedule		= rump_schedule,
	.hyp_unschedule		= rump_unschedule,
	.hyp_backend_unschedule	= rump_libos_user_unschedule,
	.hyp_backend_schedule	= rump_libos_user_schedule,
	.hyp_lwproc_switch	= rump_libos_lwproc_switch,
	.hyp_lwproc_release	= rump_libos_lwproc_release,
	.hyp_lwproc_newlwp	= rump_libos_lwproc_newlwp,
	.hyp_lwproc_curlwp	= rump_libos_lwproc_curlwp,

	.hyp_getpid		= rump_libos_hyp_getpid,
	.hyp_syscall		= rump_libos_hyp_syscall,
	.hyp_lwproc_rfork	= rump_libos_lwproc_rfork,
	.hyp_lwpexit		= rump_libos_hyp_lwpexit,
	.hyp_execnotify		= rump_libos_hyp_execnotify,
};


