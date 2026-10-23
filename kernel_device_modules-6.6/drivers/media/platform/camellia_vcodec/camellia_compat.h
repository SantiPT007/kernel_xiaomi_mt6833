/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __CAMELLIA_COMPAT_H__
#define __CAMELLIA_COMPAT_H__

#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <media/videobuf2-v4l2.h>

struct timeval {
	long tv_sec;
	long tv_usec;
};

static inline void do_gettimeofday(struct timeval *tv)
{
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
}

static inline struct vb2_buffer *camellia_vb2(struct vb2_v4l2_buffer *vbuf)
{
	return vbuf ? &vbuf->vb2_buf : NULL;
}

#endif
