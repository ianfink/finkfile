/*
 * File:		finkfile.c
 *
 * Purpose:		NetBSD device driver for circular queue file
 */

/*
 * Copyright (C) 2016 Ian M. Fink. All rights reserved.
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * 
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 * 
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 *  3. All advertising materials mentioning features or use of this software must
 *     display the following acknowledgement: 
 *        This product includes software developed by Ian M. Fink
 * 
 *  4. Neither Ian M. Fink nor the names of its contributors may be used to
 *     endorse or promote products derived from this software without specific
 *     prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY COPYRIGHT HOLDER "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IAN M. FINK BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, * DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Includes
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/lkm.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/uio.h>

/*
 * Marcros
 */

#define MAX_BUF				20
#define MAX_FINKFILES		4

/*
 * Structs
 */

struct finkfile_queue {
	u_long	head, tail;
	int		qfull;
	char	*data;
};

struct finkfile_softc {
	struct finkfile_queue		ffq;
};

/*
 * Globals
 */

static char *finkfile_buf;
static struct finkfile_queue	ffq;
static struct finkfile_softc finkfile_scs[MAX_FINKFILES];


/*
 * Typedefs
 */

typedef void * caddr_t;

/*
 * Protos
 */

int finkfile_lkmentry(struct lkm_table *lkmtp, int, int);
static int finkfile_handle(struct lkm_table *, int);
static int finkfile_open(dev_t device, int flags, int fmt, struct lwp *process);
static int finkfile_close(dev_t device, int flags, int fmt, struct lwp *process);
static int finkfile_read(dev_t dev, struct uio *uio, int flag);
static int finkfile_write(dev_t dev, struct uio *uio, int flag);
//conf.h: int             (*d_ioctl)(dev_t, u_long, void *, int, struct lwp *);
static int finkfile_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *process);

static int ffq_enqueue(struct uio *uio, struct finkfile_queue *ffq_ptr);
static int ffq_dump_queue(struct uio *uio, struct finkfile_queue *ffq_ptr);
static int ffq_empty(struct finkfile_queue *ffq_ptr);

/* The module name, as appears in 'modstat' (and used in 'modunload'). */
/* MOD_MISC("finkfile"); */

const struct cdevsw finkfile_cdevsw = {
	finkfile_open, 
	finkfile_close, 
	finkfile_read, 
	finkfile_write, 
	finkfile_ioctl,
	nostop, 
	notty, 
	nopoll, 
	nommap, 
	nokqfilter, 
	D_OTHER,
};

MOD_DEV("finkfile", "finkfile", NULL, -1, &finkfile_cdevsw, -1);

MALLOC_DECLARE(M_FINKFILE);
MALLOC_DEFINE(M_FINKFILE, "finkfile_buffer", "buffer for finkfile");

/*************************************************************************/

static int 
finkfile_open(dev_t device, int flags, int fmt, struct lwp *process)
{
	int	error = 0;

	printf("finkfile: finkfile_open minor_no = %d\n", minor(device));

	return error;
} /* finkfile_open */

/*************************************************************************/

static int 
finkfile_close(dev_t device, int flags, int fmt, struct lwp *process)
{
	int	error = 0;

	printf("finkfile: finkfile_close minor_no = %d\n", minor(device));

	return error;
} /* finkfile_close */

/*************************************************************************/

static int
finkfile_handle(struct lkm_table *lkmtp, int cmd)
{
	int		error = 0;
	int		i;
	char	*test_string = "finkfile test\n";

	switch (cmd) {
		case LKM_E_LOAD:
			if (lkmexists(lkmtp)) {
				error = EEXIST;
			} else {
				char *cptr1, *cptr2;
				finkfile_buf = (char *)malloc(sizeof(char)*MAX_BUF, M_FINKFILE, 
					M_WAITOK);
				if (finkfile_buf == NULL) {
					printf("finkfile_buf is NULL\n");
					error = EINVAL;
					break;
				}
				for (cptr1=test_string, cptr2=finkfile_buf; *cptr1; ) {
					*cptr2++ = *cptr1++;
				}
				*cptr2 = '\0';

				for (i=0; i<MAX_FINKFILES; i++) {
					finkfile_scs[i].ffq.data = (char *)malloc(
						sizeof(char)*MAX_BUF, M_FINKFILE, M_WAITOK | M_ZERO);
					if (finkfile_scs[i].ffq.data == NULL) {
						printf("finkfile_scs[%d].ffq.data is NULL\n", i);
						for (i-=1; i>=0; i--) {
							free(finkfile_scs[i].ffq.data, M_FINKFILE);
						}
						error = EINVAL;
						break;
					}
					finkfile_scs[i].ffq.tail = 0;
					finkfile_scs[i].ffq.head = 0;
					finkfile_scs[i].ffq.qfull = 0;
				}
				printf("finkfile loaded\n");
			}
			break;
		case LKM_E_UNLOAD:
			free(finkfile_buf, M_FINKFILE);
			for (i=0; i<MAX_FINKFILES; i++) {
				free(finkfile_scs[i].ffq.data, M_FINKFILE);
			}
			printf("finkfile unloaded\n");
			break;
		default:
			error = EINVAL;
			break;
	}

	return error;
} /* finkfile_handle */

/*************************************************************************/

int
finkfile_lkmentry(struct lkm_table *lkmtp, int cmd, int ver)
{
	int error = 0;

	printf("finkfile_lkmentry\n");

	DISPATCH(lkmtp, cmd, ver, finkfile_handle, finkfile_handle, lkm_nofunc);

	return error;
} /* finkfile_lkmentry */

/*************************************************************************/

static int 
finkfile_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *process)
{
	int		error = 0;

	printf("finkfile_ioctl: cmd = %d\n", cmd);

	switch (cmd) {
		case 1:
			break;
		default:
			error = ENODEV;
	} /* switch (cmd) */

	return error;
} /* finkfile_ioctl */

/*************************************************************************/

static int 
finkfile_read(dev_t dev, struct uio *uio, int flag)
{
	int error = 0;
	int minor_no = minor(dev);
	struct finkfile_queue *ffq_ptr; 
	size_t	amount;
	char	c = 'A';
	static	int	return_zero_bytes = 0;

	printf("finkfile: read called\n-------\n");
	printf("finkfile: uio_offset = %d\n", uio->uio_offset);
	printf("finkfile: uio_resid = %d\n", uio->uio_resid);
	printf("finkfile: minor_no = %d\n", minor_no);

/*
	amount = MIN(uio->uio_resid,
		(strlen(finkfile_buf) - uio->uio_offset) ?
		(strlen(finkfile_buf) - uio->uio_offset) : 0);
	error = uiomove(finkfile_buf, amount, uio); 
*/
	/* error = uiomove(finkfile_buf + uio->uio_offset, amount, uio); */

	ffq_ptr = &(finkfile_scs[minor_no].ffq);
	if (ffq_empty(ffq_ptr)) {
		error = uiomove(&c, 0, uio); 
		/* uio->uio_iov->iov_len = 0; */
		return error;
	}

	if (return_zero_bytes) {
		error = uiomove(&c, 0, uio); 
		/* uio->uio_iov->iov_len = 0; */
		return_zero_bytes = 0;
	} else {
		ffq_dump_queue(uio, ffq_ptr);
		return_zero_bytes = 1;
	}

	if (error != 0) {
		printf("finkfile: read failed\n");
	}

	return error;
} /* finkfile_read */

/*************************************************************************/

static int 
finkfile_write(dev_t dev, struct uio *uio, int flag)
{
	int error = 0;
	int minor_no = minor(dev);
	struct finkfile_queue *ffq_ptr; 
	size_t	amount;

/*
	amount = MIN(uio->uio_iov->iov_len, MAX_BUF - 1);

	printf("finkfile: write called\n-----------\n");
	printf("finkfile: amount = %d\n", amount);
	printf("finkfile: uio_iovcnt = %d\n", uio->uio_iovcnt);
	printf("finkfile: uio_offset = %d\n", uio->uio_offset);
	printf("finkfile: uio_iovcnt->iov_len = %d\n", uio->uio_iov->iov_len);
	printf("finkfile: uio_resid = %d\n", uio->uio_resid);
	
	error = uiomove(finkfile_buf, amount, uio);

	printf("*******\n");
	printf("finkfile: uio_iovcnt = %d\n", uio->uio_iovcnt);
	printf("finkfile: uio_offset = %d\n", uio->uio_offset);
	printf("finkfile: uio_iovcnt->iov_len = %d\n", uio->uio_iov->iov_len);
	printf("finkfile: uio_resid = %d\n", uio->uio_resid);


	*(finkfile_buf + amount) = '\0';


	if (error != 0) {
		printf("finkfile: write failed\n");
	}
*/

	ffq_ptr = &(finkfile_scs[minor_no].ffq);
	ffq_enqueue(uio, ffq_ptr);

	return error;
} /* finkfile_write */

/*************************************************************************/

static int
ffq_enqueue(struct uio *uio, struct finkfile_queue *ffq_ptr)
{
	size_t			tmp_amount;
	size_t			to_end;
	size_t			next_tail;
	int				ret_val = 0;
	int				error = 0;

	while (uio->uio_iov->iov_len > 0) {

		to_end = MAX_BUF - ffq_ptr->tail;
		tmp_amount = MIN(uio->uio_iov->iov_len, to_end);

		printf("\nuio->uio_iov->iov_len = %d\n", uio->uio_iov->iov_len);
		printf("\ntmp_amount = %d\n", tmp_amount);
		printf("\nffq_ptr->tail = %d\n", ffq_ptr->tail);

		error = uiomove(ffq_ptr->data + ffq_ptr->tail, tmp_amount, uio);

		if (error != 0) {
			printf("finkfile:  ff_enqueue error LINE = %d\n", __LINE__);
			printf("finkfile:  ff_enqueue uio->uio_iov->iov_len = %d\n", 
				uio->uio_iov->iov_len);
			printf("finkfile:  ff_enqueue tmp_amount = %d\n", tmp_amount);
			printf("finkfile:  ff_enqueue ffq_ptr->head = %d ffq_ptr->tail = %d\n",
				ffq_ptr->head, ffq_ptr->tail);
			return -(__LINE__);
		}

		next_tail = tmp_amount + ffq_ptr->tail;
		if (next_tail == MAX_BUF) {
			next_tail = 0;
			ffq_ptr->qfull = 1;
		}

		ffq_ptr->tail = next_tail;

		if (ffq_ptr->qfull) {
			ffq_ptr->head = ffq_ptr->tail;
			printf("finkfile:  together\n");
		}
	}

	return ret_val;
} /* ffq_enqueue */

/*************************************************************************/

static int 
ffq_dump_queue(struct uio *uio, struct finkfile_queue *ffq_ptr)
{
	int				ret_val = 0;
	int				error = 0;

	if (ffq_ptr->qfull) {
		if (ffq_ptr->head) {
			error = uiomove(ffq_ptr->data + ffq_ptr->head, MAX_BUF - ffq_ptr->head, uio);
			if (error != 0) {
				printf("finkfile:  ffq_dump_queueue line = %d\n", __LINE__);
			}
			error = uiomove(ffq_ptr->data, ffq_ptr->tail, uio);
			if (error != 0) {
				printf("finkfile:  ffq_dump_queueue line = %d\n", __LINE__);
			}
		} else {
			error = uiomove(ffq_ptr->data, MAX_BUF, uio);
			if (error != 0) {
				printf("finkfile:  ffq_dump_queueue line = %d\n", __LINE__);
			}
		}
	} else {
		error = uiomove(ffq_ptr->data, ffq_ptr->tail, uio);
		if (error != 0) {
			printf("finkfile:  ffq_dump_queueue line = %d\n", __LINE__);
		}
		/* printf("finkfile:  ffq_dump_queue ffq_ptr->data = '%s'\n", ffq_ptr->data); */
	}

	return ret_val;
} /* ffq_dump_queue */

/*************************************************************************/

static int 
ffq_empty(struct finkfile_queue *ffq_ptr)
{
	if (!ffq_ptr->qfull && ffq.head == ffq_ptr->tail) {
		return 1;
	}

	return 0;
} /* ffq_empty */

/*************************************************************************/

/*
 * End of file:  finkfile.c
 */
