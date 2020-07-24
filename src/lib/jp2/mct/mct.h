/*
 *    Copyright (C) 2016-2020 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *    This source code incorporates work covered by the following copyright and
 *    permission notice:
 *
 * The copyright in this software is being made available under the 2-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux
 * Copyright (c) 2003-2014, Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2008, 2011-2012, Centre National d'Etudes Spatiales (CNES), FR
 * Copyright (c) 2012, CS Systemes d'Information, France
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
namespace grk {

class mct {

public:

/**
 @file mct.h
 @brief Implementation of a multi-component transforms (MCT)

 The functions in MCT.C have for goal to realize reversible and irreversible multicomponent
 transform. The functions in MCT.C are used by some function in TCD.C.
 */

/** @defgroup MCT MCT - Implementation of a multi-component transform */
/*@{*/

/** @name Exported functions */
/*@{*/
/* ----------------------------------------------------------------------- */
	/**
	 Apply a reversible multi-component transform to an image
	 @param c0 Samples for red component
	 @param c1 Samples for green component
	 @param c2 Samples blue component
	 @param n Number of samples for each component
	 */
	static void encode_rev(int32_t *c0, int32_t *c1, int32_t *c2, uint64_t n);
	/**
	 Apply a reversible multi-component inverse transform to an image
	 @param c0 Samples for luminance component
	 @param c1 Samples for red chrominance component
	 @param c2 Samples for blue chrominance component
	 @param n Number of samples for each component
	 */
	static void decode_rev(int32_t *c0, int32_t *c1, int32_t *c2, uint64_t n);

	/**
	 Get wavelet norms for reversible transform
	 */
	static const double* get_norms_rev(void);


	/**
	 Apply an irreversible multi-component transform to an image
	 @param c0 Samples for red component
	 @param c1 Samples for green component
	 @param c2 Samples blue component
	 @param n Number of samples for each component
	 */
	static void encode_irrev(int32_t *c0, int32_t *c1, int32_t *c2, uint64_t n);
	/**
	 Apply an irreversible multi-component inverse transform to an image
	 @param c0 Samples for luminance component
	 @param c1 Samples for red chrominance component
	 @param c2 Samples for blue chrominance component
	 @param n Number of samples for each component
	 */
	static void decode_irrev(float *c0, float *c1, float *c2, uint64_t n);

	/**
	 Get wavelet norms for irreversible transform
	 */
	static const double* get_norms_irrev(void);

	/**
	 Custom MCT transform
	 @param p_coding_data    MCT data
	 @param n                size of components
	 @param p_data           components
	 @param nb_comp          nb of components (i.e. size of p_data)
	 @param is_signed        indicates if the data is signed
	 @return false if function encounter a problem, true otherwise
	 */
	static bool encode_custom(uint8_t *p_coding_data, uint64_t n, uint8_t **p_data,
			uint32_t nb_comp, uint32_t is_signed);
	/**
	 Custom MCT decode
	 @param pDecodingData    MCT data
	 @param n                size of components
	 @param pData            components
	 @param pNbComp          nb of components (i.e. size of p_data)
	 @param isSigned         tells if the data is signed
	 @return false if function encounter a problem, true otherwise
	 */
	static bool decode_custom(uint8_t *pDecodingData, uint64_t n, uint8_t **pData,
			uint32_t pNbComp, uint32_t isSigned);
	/**
	 Calculate norm of MCT transform
	 @param pNorms         MCT data
	 @param nb_comps       number of components
	 @param pMatrix        components
	 */
	static void calculate_norms(double *pNorms, uint32_t nb_comps, float *pMatrix);

};

/* ----------------------------------------------------------------------- */
/*@}*/

/*@}*/

}
