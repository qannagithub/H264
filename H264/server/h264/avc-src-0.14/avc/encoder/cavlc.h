/*****************************************************************************
 *
 *  T264 AVC CODEC
 *
 *  Copyright(C) 2004-2005 llcc <lcgate1@yahoo.com.cn>
 *               2004-2005 visionany <visionany@yahoo.com.cn>
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 ****************************************************************************/

#ifndef _CAVLC_H_
#define _CAVLC_H_

int8_t T264_mb_predict_intra4x4_mode(T264_t *t, int32_t idx);
void T264_macroblock_write_cavlc(T264_t *h);
uint8_t T264_mb_predict_non_zero_code(T264_t* t, int idx);

#endif