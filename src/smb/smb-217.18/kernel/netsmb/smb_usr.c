/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: smb_usr.c,v 1.15.178.1 2005/11/15 01:45:30 lindak Exp $
 */
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <sys/smb_apple.h>

#include <sys/smb_iconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>

/*
 * helpers for nsmb device. Can be moved to the smb_dev.c file.
 */
static void smb_usr_vcspec_free(struct smb_vcspec *spec);

static int
smb_usr_vc2spec(struct smbioc_ossn *dp, struct smb_vcspec *spec)
{
	int flags = 0;
	int error;

	bzero(spec, sizeof(*spec));
	if (dp->ioc_server == NULL)
		return EINVAL;
	if (dp->ioc_localcs[0] == 0) {
		SMBERROR("no local charset ?\n");
		return EINVAL;
	}
	/*
	 * Something went bad in user land, just get out. This would cause a
	 * hang on Intel and a panic on PPC. See Radar 4321020 for more 
	 * information.
	 */
	if (! dp->ioc_svlen ) {
		SMBERROR("server name's length is zero ?\n");
		return EINVAL;
	}
	spec->sap = smb_memdupin(dp->ioc_server, dp->ioc_svlen);
	if (spec->sap == NULL)
		return ENOMEM;
	if (dp->ioc_local) {
		spec->lap = smb_memdupin(dp->ioc_local, dp->ioc_lolen);
		if (spec->lap == NULL) {
			smb_usr_vcspec_free(spec);
			return ENOMEM;
		}
	}
	if (dp->ioc_intok) {
		if ((error = copyin(CAST_USER_ADDR_T(dp->ioc_intok), &spec->toklen,
				    sizeof spec->toklen))) {
			smb_usr_vcspec_free(spec);
			return error;
		}
		if (spec->toklen) {
			spec->tok = malloc(spec->toklen, M_SMBSTR, M_WAITOK);
			if ((error = copyin(CAST_USER_ADDR_T((char *)dp->ioc_intok +
						    sizeof spec->toklen),
					    spec->tok, spec->toklen))) {
				smb_usr_vcspec_free(spec);
				return error;
			}
		}
	}
	spec->srvname = dp->ioc_srvname;
	spec->pass = dp->ioc_password;
	spec->domain = dp->ioc_workgroup;
	spec->username = dp->ioc_user;
	spec->mode = dp->ioc_mode;
	spec->rights = dp->ioc_rights;
	spec->owner = dp->ioc_owner;
	spec->group = dp->ioc_group;
	spec->localcs = dp->ioc_localcs;
	spec->servercs = dp->ioc_servercs;
	if (dp->ioc_opt & SMBVOPT_PRIVATE)
		flags |= SMBV_PRIVATE;
	if (dp->ioc_opt & SMBVOPT_SINGLESHARE)
		flags |= SMBV_PRIVATE | SMBV_SINGLESHARE;
	if (dp->ioc_opt & SMBVOPT_EXT_SEC)
		flags |= SMBV_EXT_SEC;
	switch (dp->ioc_opt & SMBVOPT_MINAUTH) {

	case SMBVOPT_MINAUTH_NONE:
		/* this is probably optimized away */
		flags |= SMBV_MINAUTH_NONE;
		break;

	case SMBVOPT_MINAUTH_LM:
		flags |= SMBV_MINAUTH_LM;
		break;

	case SMBVOPT_MINAUTH_NTLM:
		flags |= SMBV_MINAUTH_NTLM;
		break;

	case SMBVOPT_MINAUTH_NTLMV2:
		flags |= SMBV_MINAUTH_NTLMV2;
		break;

	case SMBVOPT_MINAUTH_KERBEROS:
		flags |= SMBV_MINAUTH_KERBEROS;
		break;

	default:
		smb_usr_vcspec_free(spec);
		return EINVAL;
	}
	spec->flags = flags;
	return 0;
}

static void
smb_usr_vcspec_free(struct smb_vcspec *spec)
{
	if (spec->sap)
		smb_memfree(spec->sap);
	spec->sap = NULL;
	if (spec->lap)
		smb_memfree(spec->lap);
	spec->lap = NULL;
	if (spec->tok)
		smb_memfree(spec->tok);
	spec->tok = NULL;
}

static int
smb_usr_share2spec(struct smbioc_oshare *dp, struct smb_sharespec *spec)
{
	bzero(spec, sizeof(*spec));
	spec->mode = dp->ioc_mode;
	spec->rights = dp->ioc_rights;
	spec->owner = dp->ioc_owner;
	spec->group = dp->ioc_group;
	spec->name = dp->ioc_share;
	spec->stype = dp->ioc_stype;
	spec->pass = dp->ioc_password;
	return 0;
}


int
smb_usr_negotiate(struct smbioc_lookup *dp, struct smb_cred *scred,
		  struct smb_vc **vcpp, struct smb_share **sspp)
{
	struct smb_vc *vcp = NULL;
	struct smb_vcspec vspec;
	struct smb_sharespec sspec, *sspecp = NULL;
	int error;
	size_t t;

	if (dp->ioc_level < SMBL_VC || dp->ioc_level > SMBL_SHARE)
		return EINVAL;
	error = smb_usr_vc2spec(&dp->ioc_ssn, &vspec);
	if (error)
		return error;
	if (dp->ioc_flags & SMBLK_CREATE)
		vspec.flags |= SMBV_CREATE;
	if (dp->ioc_level >= SMBL_SHARE) {
		error = smb_usr_share2spec(&dp->ioc_sh, &sspec);
		if (error)
			goto out;
		sspecp = &sspec;
	}
	error = smb_sm_negotiate(&vspec, sspecp, scred, &vcp);
	if (error == 0) {
		*vcpp = vcp;
		*sspp = vspec.ssp;
		if (dp->ioc_ssn.ioc_outtok &&
		    !(error = copyin(CAST_USER_ADDR_T(dp->ioc_ssn.ioc_outtok), &t, sizeof t)) &&
		    !(error = copyout(&vcp->vc_outtoklen,
				      CAST_USER_ADDR_T(dp->ioc_ssn.ioc_outtok),
				      sizeof(size_t))) &&
		    vcp->vc_outtoklen && vcp->vc_outtoklen <= t &&
		    !(error = copyout(vcp->vc_outtok,
				      CAST_USER_ADDR_T((char *)dp->ioc_ssn.ioc_outtok +
							 sizeof(size_t)),
				      vcp->vc_outtoklen))) {
			/*
			 * Save this blob in vc_negtok.  We need it in
			 * case we have to reconnect
			 */
			if (vcp->vc_negtok)
				free(vcp->vc_negtok, M_SMBTEMP);
			vcp->vc_negtok = vcp->vc_outtok;
			vcp->vc_outtok = NULL;
			vcp->vc_negtoklen = vcp->vc_outtoklen;
			vcp->vc_outtoklen = 0;
		}
	}
out:
	smb_usr_vcspec_free(&vspec);
	return error;
}

int
smb_usr_ssnsetup(struct smbioc_lookup *dp, struct smb_cred *scred,
	struct smb_vc *vcp, struct smb_share **sspp)
{
	struct smb_vcspec vspec;
	struct smb_sharespec sspec, *sspecp = NULL;
	int error;
	size_t t;

	if (dp->ioc_level < SMBL_VC || dp->ioc_level > SMBL_SHARE)
		return EINVAL;
	error = smb_usr_vc2spec(&dp->ioc_ssn, &vspec);
	if (error)
		return error;
	if (dp->ioc_level >= SMBL_SHARE) {
		error = smb_usr_share2spec(&dp->ioc_sh, &sspec);
		if (error)
			goto out;
		sspecp = &sspec;
	}
	error = smb_sm_ssnsetup(&vspec, sspecp, scred, vcp);
	if (error == 0) {
		*sspp = vspec.ssp;
		if (vcp->vc_outtoklen &&
		    !(error = copyin(CAST_USER_ADDR_T(dp->ioc_ssn.ioc_outtok), &t, sizeof t)) &&
		    !(error = copyout(&vcp->vc_outtoklen,
				      CAST_USER_ADDR_T(dp->ioc_ssn.ioc_outtok),
				      sizeof(size_t))) &&
		    vcp->vc_outtoklen <= t &&
		    !(error = copyout(vcp->vc_outtok,
				      CAST_USER_ADDR_T((char *)dp->ioc_ssn.ioc_outtok +
							 sizeof(size_t)),
				      vcp->vc_outtoklen))) {
			vcp->vc_outtoklen = 0;
			free(vcp->vc_outtok, M_SMBTEMP);
			vcp->vc_outtok = NULL;
		}
	}
out:
	smb_usr_vcspec_free(&vspec);
	return error;
}


int
smb_usr_tcon(struct smbioc_lookup *dp, struct smb_cred *scred,
	struct smb_vc *vcp, struct smb_share **sspp)
{
	struct smb_vcspec vspec;
	struct smb_sharespec sspec, *sspecp = NULL;
	int error;

	if (dp->ioc_level < SMBL_VC || dp->ioc_level > SMBL_SHARE)
		return EINVAL;
	error = smb_usr_vc2spec(&dp->ioc_ssn, &vspec);
	if (error)
		return error;
	if (dp->ioc_level >= SMBL_SHARE) {
		error = smb_usr_share2spec(&dp->ioc_sh, &sspec);
		if (error)
			goto out;
		sspecp = &sspec;
	}
	error = smb_sm_tcon(&vspec, sspecp, scred, vcp);
	if (error == 0) {
		*sspp = vspec.ssp;
	}
out:
	smb_usr_vcspec_free(&vspec);
	return error;
}

/*
 * Connect to the resource specified by smbioc_ossn structure.
 * It may either find an existing connection or try to establish a new one.
 * If no errors occured smb_vc returned locked and referenced.
 */

int
smb_usr_simplerequest(struct smb_share *ssp, struct smbioc_rq *dp,
	struct smb_cred *scred)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	u_int16_t bc;
	int error;

	switch (dp->ioc_cmd) {
	    case SMB_COM_TRANSACTION2:
	    case SMB_COM_TRANSACTION2_SECONDARY:
	    case SMB_COM_CLOSE_AND_TREE_DISC:
	    case SMB_COM_TREE_CONNECT:
	    case SMB_COM_TREE_DISCONNECT:
	    case SMB_COM_NEGOTIATE:
	    case SMB_COM_SESSION_SETUP_ANDX:
	    case SMB_COM_LOGOFF_ANDX:
	    case SMB_COM_TREE_CONNECT_ANDX:
		return EPERM;
	}
	error = smb_rq_init(rqp, SSTOCP(ssp), dp->ioc_cmd, scred);
	if (error)
		return error;
	mbp = &rqp->sr_rq;
	smb_rq_wstart(rqp);
	error = mb_put_mem(mbp, dp->ioc_twords, dp->ioc_twc * 2, MB_MUSER);
	if (error)
		goto bad;
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	error = mb_put_mem(mbp, dp->ioc_tbytes, dp->ioc_tbc, MB_MUSER);
	if (error)
		goto bad;
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	if (error)
		goto bad;
	mdp = &rqp->sr_rp;
	md_get_uint8(mdp, &wc);
	dp->ioc_rwc = wc;
	wc *= 2;
	if (wc > dp->ioc_rpbufsz) {
		error = EBADRPC;
		goto bad;
	}
	error = md_get_mem(mdp, dp->ioc_rpbuf, wc, MB_MUSER);
	if (error)
		goto bad;
	md_get_uint16le(mdp, &bc);
	if ((wc + bc) > dp->ioc_rpbufsz) {
		error = EBADRPC;
		goto bad;
	}
	dp->ioc_rbc = bc;
	error = md_get_mem(mdp, dp->ioc_rpbuf + wc, bc, MB_MUSER);
bad:
	dp->ioc_errclass = rqp->sr_errclass;
	dp->ioc_serror = rqp->sr_serror;
	dp->ioc_error = rqp->sr_error;
	smb_rq_done(rqp);
	return error;

}

static int
smb_cpdatain(struct mbchain *mbp, int len, caddr_t data)
{
	int error;

	if (len == 0)
		return 0;
	error = mb_init(mbp);
	if (error)
		return error;
	return mb_put_mem(mbp, data, len, MB_MUSER);
}

int
smb_usr_t2request(struct smb_share *ssp, struct smbioc_t2rq *dp,
	struct smb_cred *scred)
{
	struct smb_t2rq t2, *t2p = &t2;
	struct mdchain *mdp;
	int error, len;

	if (dp->ioc_setupcnt > SMB_MAXSETUPWORDS)
		return EINVAL;
	error = smb_t2_init(t2p, SSTOCP(ssp), dp->ioc_setup, dp->ioc_setupcnt,
	    scred);
	if (error)
		return error;
	len = t2p->t2_setupcount = dp->ioc_setupcnt;
	if (len > 1)
		t2p->t2_setupdata = dp->ioc_setup; 
	if (dp->ioc_name) {
		t2p->t_name = smb_strdupin(dp->ioc_name, 128);
		if (t2p->t_name == NULL) {
			error = ENOMEM;
			goto bad;
		}
	}
	t2p->t2_maxscount = 0;
	t2p->t2_maxpcount = dp->ioc_rparamcnt;
	t2p->t2_maxdcount = dp->ioc_rdatacnt;
	error = smb_cpdatain(&t2p->t2_tparam, dp->ioc_tparamcnt, dp->ioc_tparam);
	if (error)
		goto bad;
	error = smb_cpdatain(&t2p->t2_tdata, dp->ioc_tdatacnt, dp->ioc_tdata);
	if (error)
		goto bad;
	error = smb_t2_request(t2p);
	dp->ioc_errclass = t2p->t2_sr_errclass;
	dp->ioc_serror = t2p->t2_sr_serror;
	dp->ioc_error = t2p->t2_sr_error;
	dp->ioc_rpflags2 = t2p->t2_sr_rpflags2;
	if (error)
		goto bad;
	mdp = &t2p->t2_rparam;
	if (mdp->md_top) {
		len = m_fixhdr(mdp->md_top);
		if (len > dp->ioc_rparamcnt) {
			error = EMSGSIZE;
			goto bad;
		}
		dp->ioc_rparamcnt = len;
		error = md_get_mem(mdp, dp->ioc_rparam, len, MB_MUSER);
		if (error)
			goto bad;
	} else
		dp->ioc_rparamcnt = 0;
	mdp = &t2p->t2_rdata;
	if (mdp->md_top) {
		len = m_fixhdr(mdp->md_top);
		if (len > dp->ioc_rdatacnt) {
			error = EMSGSIZE;
			goto bad;
		}
		dp->ioc_rdatacnt = len;
		error = md_get_mem(mdp, dp->ioc_rdata, len, MB_MUSER);
	} else
		dp->ioc_rdatacnt = 0;
bad:
	if (t2p->t_name)
		smb_strfree(t2p->t_name);
	smb_t2_done(t2p);
	return error;
}
