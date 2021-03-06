/******************************************************************************
 ** Copyright (c) 2015-2016, Intel Corporation                                **
 ** All rights reserved.                                                      **
 **                                                                           **
 ** Redistribution and use in source and binary forms, with or without        **
 ** modification, are permitted provided that the following conditions        **
 ** are met:                                                                  **
 ** 1. Redistributions of source code must retain the above copyright         **
 **    notice, this list of conditions and the following disclaimer.          **
 ** 2. Redistributions in binary form must reproduce the above copyright      **
 **    notice, this list of conditions and the following disclaimer in the    **
 **    documentation and/or other materials provided with the distribution.   **
 ** 3. Neither the name of the copyright holder nor the names of its          **
 **    contributors may be used to endorse or promote products derived        **
 **    from this software without specific prior written permission.          **
 **                                                                           **
 ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
 ** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
 ** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
 ** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
 ** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
 ** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
 ** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
 ** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
 ** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
 ** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
 ** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
 ******************************************************************************/
/* Rajkishore Barik (Intel Corp)
 ******************************************************************************/

#include "generator_convolution_backward_avx512.h"
#include "generator_convolution_common.h"
#include "generator_x86_instructions.h"
#include "generator_common.h"

#include <libxsmm_macros.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

LIBXSMM_INTERNAL_API_DEFINITION
void libxsmm_generator_convolution_backward_avx512_kernel( libxsmm_generated_code*           io_generated_code,
                                                           const libxsmm_convolution_backward_descriptor* i_conv_desc,
                                                           const char*                       i_arch ) {
  libxsmm_convolution_kernel_config l_conv_kernel_config;
  libxsmm_convolution_backward_gp_reg_mapping l_gp_reg_mapping;
  libxsmm_loop_label_tracker l_loop_label_tracker;

  unsigned int l_kw_trips = 1, l_kh_trips = 1;
  unsigned int l_ofw_trips = 1;
  unsigned int oi = 0;
#ifdef OLD_PREFETCH_OUTPUT_INPUT_L2
  unsigned int j, k, reg_count;
  unsigned int l_reg_per_block;
#endif
#if 0
  unsigned int i_ofm=0;
  unsigned int oi_=0;
  unsigned int ki=0, kj=0;
  unsigned int ofw_reg_block_factor = i_conv_desc->ofw_rb;
  unsigned int l_ofw=0;
  unsigned int l_ki=0, l_kj=0;
#endif
  /* define gp register mapping */
  /* NOTE: do not use RSP, RBP,
     do not use don't use R12 and R13 for addresses will add 4 bytes to the instructions as they are in the same line as rsp and rbp */
  libxsmm_reset_x86_convolution_backward_gp_reg_mapping( &l_gp_reg_mapping );
  l_gp_reg_mapping.gp_reg_input = LIBXSMM_X86_GP_REG_RDI;
  l_gp_reg_mapping.gp_reg_weight = LIBXSMM_X86_GP_REG_RSI;
  l_gp_reg_mapping.gp_reg_output = LIBXSMM_X86_GP_REG_RDX;
  l_gp_reg_mapping.gp_reg_input_pf = LIBXSMM_X86_GP_REG_RCX;
  l_gp_reg_mapping.gp_reg_weight_pf = LIBXSMM_X86_GP_REG_R8;
  l_gp_reg_mapping.gp_reg_output_pf = LIBXSMM_X86_GP_REG_R9;

  l_gp_reg_mapping.gp_reg_kw_loop = LIBXSMM_X86_GP_REG_R15;
  l_gp_reg_mapping.gp_reg_oi_loop = LIBXSMM_X86_GP_REG_R12;
  l_gp_reg_mapping.gp_reg_kh_loop = LIBXSMM_X86_GP_REG_UNDEF;
  l_gp_reg_mapping.gp_reg_ofmInner_loop = LIBXSMM_X86_GP_REG_UNDEF;
  l_gp_reg_mapping.gp_reg_help_0 = LIBXSMM_X86_GP_REG_RAX;
  l_gp_reg_mapping.gp_reg_help_1 = LIBXSMM_X86_GP_REG_RBX;
  l_gp_reg_mapping.gp_reg_help_2 = LIBXSMM_X86_GP_REG_R10;
  l_gp_reg_mapping.gp_reg_help_3 = LIBXSMM_X86_GP_REG_R11;
  l_gp_reg_mapping.gp_reg_help_4 = LIBXSMM_X86_GP_REG_R13;
  l_gp_reg_mapping.gp_reg_help_5 = LIBXSMM_X86_GP_REG_R14;
  l_gp_reg_mapping.gp_reg_help_6 = LIBXSMM_X86_GP_REG_R15;

  /* define convolution kernel config */
  libxsmm_generator_init_convolution_kernel_config( &l_conv_kernel_config );
  if ( strcmp( i_arch, "knl" ) == 0 ) {
    l_conv_kernel_config.instruction_set = LIBXSMM_X86_AVX512_MIC;
  }
  l_conv_kernel_config.vector_reg_count = 32;
  l_conv_kernel_config.vector_length = 16;
  l_conv_kernel_config.datatype_size = 4;
  l_conv_kernel_config.vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
  l_conv_kernel_config.vfma_instruction = LIBXSMM_X86_INSTR_VFMADD231PS;
  l_conv_kernel_config.vxor_instruction = LIBXSMM_X86_INSTR_VPXORD;
  l_conv_kernel_config.vadd_instruction = LIBXSMM_X86_INSTR_VADDPS;
  l_conv_kernel_config.prefetch_instruction = LIBXSMM_X86_INSTR_PREFETCHT2;
  l_conv_kernel_config.alu_add_instruction = LIBXSMM_X86_INSTR_ADDQ;
  l_conv_kernel_config.alu_sub_instruction = LIBXSMM_X86_INSTR_SUBQ;
  l_conv_kernel_config.alu_cmp_instruction = LIBXSMM_X86_INSTR_CMPQ;
  l_conv_kernel_config.alu_jmp_instruction = LIBXSMM_X86_INSTR_JL;
  l_conv_kernel_config.alu_mov_instruction = LIBXSMM_X86_INSTR_MOVQ;
  l_conv_kernel_config.vector_name = 'z';

  /* define loop_label_tracker */
  libxsmm_reset_loop_label_tracker( &l_loop_label_tracker );

  /* open asm */
  libxsmm_x86_instruction_open_stream_convolution( io_generated_code, l_gp_reg_mapping.gp_reg_input,
                                                   l_gp_reg_mapping.gp_reg_weight, l_gp_reg_mapping.gp_reg_output,
                                                   l_gp_reg_mapping.gp_reg_input_pf, l_gp_reg_mapping.gp_reg_weight_pf,
                                                   l_gp_reg_mapping.gp_reg_output_pf, i_arch );

  /**** Code sequence:
# define BP_UNROLL_FACTOR 2
  for(int oi=0; oi < ofw; oi+=BP_UNROLL_FACTOR) {
    int ii = oi;
    float (* __restrict tmpin1)[VLEN] = &del_input[img][ifm1][ij][ii];
#   pragma unroll
    for(int ki=0; ki < KW; ki++) {
      __m512 acc00 = _mm512_load_ps(&tmpin1[ki + 0]);
      __m512 acc01 = _mm512_load_ps(&tmpin1[ki + 1]);
#       pragma unroll
        for(int ofm2 = 0; ofm2 < VLEN; ofm2++) {
          __m512 weight00 = _mm512_load_ps(&tr_wt[kh-kj-1][ki][ofm2]);
          acc00 = _mm512_fmadd_ps(_mm512_set1_ps(del_output[img][ofm1][oj][oi+0][ofm2]), weight00, acc00);
          acc01 = _mm512_fmadd_ps(_mm512_set1_ps(del_output[img][ofm1][oj][oi+1][ofm2]), weight00, acc01);
        }
        _mm512_store_ps( &tmpin1[ki+ 0], acc00 );
        _mm512_store_ps( &tmpin1[ki+ 1], acc01 );
    }
  }
  *****/
#define ENABLE_INPUT_PREFETCH
#define ENABLE_OUTPUT_PREFETCH
#define ENABLE_WEIGHT_PREFETCH

#if 1
  /* initilize KW and OFW unrolling */
  if (i_conv_desc->unroll_kw != 0) {
    l_kw_trips = i_conv_desc->kw;
  } else l_kw_trips = 1;
  if (i_conv_desc->ofw_unroll != 0) {
    l_ofw_trips = i_conv_desc->ofw/i_conv_desc->ofw_rb;
  } else l_ofw_trips = 1;
#if 0
  printf("ofw=%d ofw_rb=%d\n", i_conv_desc->ofw, i_conv_desc->ofw_rb);
  printf("ofw_unroll=%d l_ofw_trips=%d\n", i_conv_desc->ofw_unroll, l_ofw_trips);
  printf("kw_unroll=%d l_kw_trips=%d\n", i_conv_desc->unroll_kw, l_kw_trips);
#endif
  if (i_conv_desc->peeled == 0) { /* NON-PEELED VERSION */
#ifdef OLD_PREFETCH_OUTPUT_INPUT_L2
    l_reg_per_block = i_conv_desc->ofm_block / l_conv_kernel_config.vector_length;
    reg_count = 0;
    for ( j = 0; j < i_conv_desc->ofw_rb; j++ ) {
      for ( k = 0; k < l_reg_per_block ; k++, reg_count++ ) {
        if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L2)) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*LIBXSMM_X86_INSTR_PREFETCHT0,*/
                                     l_conv_kernel_config.prefetch_instruction,
                                     l_gp_reg_mapping.gp_reg_output_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((reg_count)* i_conv_desc->ofm_block * l_conv_kernel_config.datatype_size) + (i_conv_desc->ofw_padded  * i_conv_desc->ofm_block * l_conv_kernel_config.datatype_size));
        }
        if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L2)) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*LIBXSMM_X86_INSTR_PREFETCHT0,*/
                                     l_conv_kernel_config.prefetch_instruction,
                                     l_gp_reg_mapping.gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((reg_count)* l_conv_kernel_config.vector_length* l_conv_kernel_config.datatype_size) + (i_conv_desc->ifw_padded  * l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size));
      }
    }
  }
#endif
  if ((i_conv_desc->ofw_unroll == 0)&& ((i_conv_desc->ofw/i_conv_desc->ofw_rb) > 1) ) {
    /* header of oi loop */
    libxsmm_generator_convolution_header_oi_loop(  io_generated_code, &l_loop_label_tracker,
                                               &l_conv_kernel_config, l_gp_reg_mapping.gp_reg_oi_loop );
  }

  /* unroll ofw */
  for ( oi = 0; oi < l_ofw_trips; oi++) {


    if ( (i_conv_desc->unroll_kw == 0) && (i_conv_desc->kw > 1) ) {
      /* open KW loop, ki */
      libxsmm_generator_convolution_header_kw_loop(  io_generated_code, &l_loop_label_tracker,
                                               &l_conv_kernel_config, l_gp_reg_mapping.gp_reg_kw_loop );

    }

    /* ifmInner loop, VLEN, ifm2, fully unrolled blocked by ofw_rb * ofw_rb */
    libxsmm_generator_convolution_backward_avx512_ofmloop(  io_generated_code,
                                                          &l_gp_reg_mapping,
                                                          &l_conv_kernel_config,
                                                           i_conv_desc,
                                                           i_conv_desc->unroll_kw == 0 ? 1 : l_kw_trips);

     if ( (i_conv_desc->unroll_kw == 0) && (i_conv_desc->kw > 1)) {
      /* Add 40 to input */
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_input,
                                     l_conv_kernel_config.vector_length*l_conv_kernel_config.datatype_size  );

      /* Add 400 to weight */
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_weight,
                                     i_conv_desc->ofm_block* l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size  );
#ifdef ENABLE_INPUT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_input_pf,
                                     l_conv_kernel_config.vector_length*l_conv_kernel_config.datatype_size  );
      }
#endif
#ifdef ENABLE_WEIGHT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_weight_pf,
                                     i_conv_desc->ofm_block* l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size  );
      }
#endif
      /* close KW loop, ki */
      libxsmm_generator_convolution_footer_kw_loop(  io_generated_code, &l_loop_label_tracker,
                                                    &l_conv_kernel_config, l_gp_reg_mapping.gp_reg_kw_loop, i_conv_desc->kw /*l_kw_trips*/ );
      /* Substract kw * 40 to input */
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_sub_instruction,
                                     l_gp_reg_mapping.gp_reg_input,
                                     i_conv_desc->kw *  l_conv_kernel_config.vector_length*l_conv_kernel_config.datatype_size  );

      /* Substract 400 to weight */
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_sub_instruction,
                                     l_gp_reg_mapping.gp_reg_weight,
                                     i_conv_desc->kw * i_conv_desc->ofm_block* l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size  );
#ifdef ENABLE_INPUT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_sub_instruction,
                                     l_gp_reg_mapping.gp_reg_input_pf,
                                     i_conv_desc->kw * l_conv_kernel_config.vector_length*l_conv_kernel_config.datatype_size  );
      }
#endif
#ifdef ENABLE_WEIGHT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_sub_instruction,
                                     l_gp_reg_mapping.gp_reg_weight_pf,
                                     i_conv_desc->kw * i_conv_desc->ofm_block* l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size  );
      }
#endif
    }

    if(((i_conv_desc->ofw_unroll == 0) && ((i_conv_desc->ofw/i_conv_desc->ofw_rb) > 1)) || (l_ofw_trips > 1)) {
      /* Add 40 to input, output */
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_input,
                                     i_conv_desc->ofw_rb * /** i_conv_desc->kw * */ l_conv_kernel_config.vector_length  /* * i_conv_desc->ofm_block*/  * l_conv_kernel_config.datatype_size  );
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_output,
                                     i_conv_desc->ofw_rb *  i_conv_desc->ofm_block * l_conv_kernel_config.datatype_size  );

#ifdef ENABLE_INPUT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_input_pf,
                                     i_conv_desc->ofw_rb * l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size  );
      }
#endif
#ifdef ENABLE_OUTPUT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_output_pf,
                                     i_conv_desc->ofw_rb * i_conv_desc->ofm_block *l_conv_kernel_config.datatype_size  );
      }
#endif
    }
  }

  if ((i_conv_desc->ofw_unroll == 0) && ((i_conv_desc->ofw/i_conv_desc->ofw_rb) > 1)) {
    /* close oi loop with blocking */
    libxsmm_generator_convolution_footer_oi_loop(  io_generated_code, &l_loop_label_tracker,
                                                  &l_conv_kernel_config, l_gp_reg_mapping.gp_reg_oi_loop, i_conv_desc->ofw/i_conv_desc->ofw_rb );
  }

  } else { /* PEELED Version */

    if (i_conv_desc->unroll_kh != 0) {
      l_kh_trips = i_conv_desc->kh;
    } else l_kh_trips = 1;

#ifdef OLD_PREFETCH_INPUT_OUTPUT_L2
    l_reg_per_block = i_conv_desc->ofm_block / l_conv_kernel_config.vector_length;
    reg_count = 0;
    for ( j = 0; j < i_conv_desc->ofw_rb; j++ ) {
      for ( k = 0; k < l_reg_per_block ; k++, reg_count++ ) {
        if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L2)) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*LIBXSMM_X86_INSTR_PREFETCHT0,*/
                                     l_conv_kernel_config.prefetch_instruction,
                                     l_gp_reg_mapping.gp_reg_output_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((reg_count)* i_conv_desc->ofm_block * l_conv_kernel_config.datatype_size) + (i_conv_desc->ofw_padded  * i_conv_desc->ofm_block * l_conv_kernel_config.datatype_size));
        }
        if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L2)) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*LIBXSMM_X86_INSTR_PREFETCHT0,*/
                                     l_conv_kernel_config.prefetch_instruction,
                                     l_gp_reg_mapping.gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((reg_count)* l_conv_kernel_config.vector_length* l_conv_kernel_config.datatype_size) + (i_conv_desc->ifw_padded  * l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size));
      }
    }
  }
#endif

  if ((i_conv_desc->ofw_unroll == 0) && ((i_conv_desc->ofw/i_conv_desc->ofw_rb) > 1) ) {
    /* header of oi loop */
    libxsmm_generator_convolution_header_oi_loop(  io_generated_code, &l_loop_label_tracker,
                                               &l_conv_kernel_config, l_gp_reg_mapping.gp_reg_oi_loop );
  }

  /* unroll ofw */
  for ( oi = 0; oi < l_ofw_trips; oi++) {


    if ( (i_conv_desc->unroll_kw == 0) && (i_conv_desc->kw > 1) ) {
      /* open KW loop, ki */
      libxsmm_generator_convolution_header_kw_loop(  io_generated_code, &l_loop_label_tracker,
                                               &l_conv_kernel_config, l_gp_reg_mapping.gp_reg_kw_loop );
    }


      /* ifmInner loop, VLEN, ifm2, fully unrolled blocked by ofw_rb * ofw_rb */
      libxsmm_generator_convolution_backward_avx512_ofmloop_peeled(  io_generated_code,
                                                            &l_gp_reg_mapping,
                                                            &l_conv_kernel_config,
                                                             i_conv_desc,
                                                             i_conv_desc->unroll_kw == 0 ? 1 : l_kw_trips, i_conv_desc->unroll_kh == 0 ? 1 : l_kh_trips);


     if ( (i_conv_desc->unroll_kw == 0) && (i_conv_desc->kw > 1)) { /* FIXME: Add prefetch for non-unrolled version */
      /* Add 40 to input */
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_input,
                                     l_conv_kernel_config.vector_length*l_conv_kernel_config.datatype_size  );

      /* Add 400 to weight */
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_weight,
                                     i_conv_desc->ofm_block* l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size  );
#ifdef ENABLE_INPUT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_input_pf,
                                     l_conv_kernel_config.vector_length*l_conv_kernel_config.datatype_size  );
      }
#endif
#ifdef ENABLE_WEIGHT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_weight_pf,
                                     i_conv_desc->ofm_block* l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size  );
      }
#endif
      /* close KW loop, ki */
      libxsmm_generator_convolution_footer_kw_loop(  io_generated_code, &l_loop_label_tracker,
                                                    &l_conv_kernel_config, l_gp_reg_mapping.gp_reg_kw_loop, i_conv_desc->kw  );
      /* Substract kw * 40 to input */
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_sub_instruction,
                                     l_gp_reg_mapping.gp_reg_input,
                                     i_conv_desc->kw *  l_conv_kernel_config.vector_length*l_conv_kernel_config.datatype_size  );

      /* Substract 400 to weight */
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_sub_instruction,
                                     l_gp_reg_mapping.gp_reg_weight,
                                     i_conv_desc->kw * i_conv_desc->ofm_block* l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size  );
#ifdef ENABLE_INPUT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_sub_instruction,
                                     l_gp_reg_mapping.gp_reg_input_pf,
                                     i_conv_desc->kw * l_conv_kernel_config.vector_length*l_conv_kernel_config.datatype_size  );
      }
#endif
#ifdef ENABLE_WEIGHT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_sub_instruction,
                                     l_gp_reg_mapping.gp_reg_weight_pf,
                                     i_conv_desc->kw * i_conv_desc->ofm_block* l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size  );
      }
#endif
    }

    if(((i_conv_desc->ofw_unroll == 0) && ((i_conv_desc->ofw/i_conv_desc->ofw_rb) > 1)) || (l_ofw_trips > 1)) {
      /* Add 40 to input, output */
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_input,
                                     i_conv_desc->ofw_rb * /** i_conv_desc->kw * */ l_conv_kernel_config.vector_length  /* * i_conv_desc->ofm_block*/  * l_conv_kernel_config.datatype_size  );
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_output,
                                     i_conv_desc->ofw_rb *  i_conv_desc->ofm_block * l_conv_kernel_config.datatype_size  );
#ifdef ENABLE_INPUT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_input_pf,
                                     i_conv_desc->ofw_rb * l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size  );
      }
#endif
#ifdef ENABLE_OUTPUT_PREFETCH
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) {
        libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     l_conv_kernel_config.alu_add_instruction,
                                     l_gp_reg_mapping.gp_reg_output_pf,
                                     i_conv_desc->ofw_rb * i_conv_desc->ofm_block *l_conv_kernel_config.datatype_size  );
      }
#endif
    }
  }

  if ((i_conv_desc->ofw_unroll == 0) && ((i_conv_desc->ofw/i_conv_desc->ofw_rb) > 1)) {
    /* close oi loop with blocking */
    libxsmm_generator_convolution_footer_oi_loop(  io_generated_code, &l_loop_label_tracker,
                                                  &l_conv_kernel_config, l_gp_reg_mapping.gp_reg_oi_loop, i_conv_desc->ofw/i_conv_desc->ofw_rb );
  }

  } /* End of Peeled version */

#else
#if 0
  printf("ofw_reg_block_factor=%d reg_count=%d\n", ofw_reg_block_factor, l_conv_kernel_config.vector_reg_count);
#endif
  if(i_conv_desc->ofw_unroll && (i_conv_desc->ofw % ofw_reg_block_factor == 0)) {
    for(oi=0; oi<i_conv_desc->ofw; oi+= ofw_reg_block_factor) {
      /* TODO: generate code for the multiples */
      /* check if kw needs to be unrolled */
      if(i_conv_desc->unroll_kw) {
        for(ki=0; ki<i_conv_desc->kw; ki++) {
          for(oi_=0; oi_<ofw_reg_block_factor; oi_++) {
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                    l_conv_kernel_config.instruction_set,
                                    l_conv_kernel_config.vmove_instruction,
                                    l_gp_reg_mapping.gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    (oi+oi_+ki)*l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size,
                                    l_conv_kernel_config.vector_name,
                                    l_conv_kernel_config.vector_reg_count-oi_-1 , 0, 0 /*load*/);
          }
          /* fully unroll the ofm loop that only iterates 16 times*/
          for(i_ofm=0; i_ofm<i_conv_desc->ofm_block; i_ofm++) {
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                    l_conv_kernel_config.instruction_set,
                                    l_conv_kernel_config.vmove_instruction,
                                    l_gp_reg_mapping.gp_reg_weight,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    (ki*i_conv_desc->ofm_block + i_ofm)*l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size,
                                    l_conv_kernel_config.vector_name,
                                    l_conv_kernel_config.vector_reg_count-ofw_reg_block_factor-1 , 0, 0 /*load*/);
            for(oi_=0; oi_<ofw_reg_block_factor; oi_++) {
              /* fma */
              libxsmm_x86_instruction_vec_compute_mem( io_generated_code,
                                           l_conv_kernel_config.instruction_set,
                                           l_conv_kernel_config.vfma_instruction,
                                           1, /* use broadcast*/
                                           l_gp_reg_mapping.gp_reg_output,
                                           LIBXSMM_X86_GP_REG_UNDEF /* for not using SIB addressing */,
                                           0 /* no scale for no SIB addressing */,
                                           /* disp */ ((oi+oi_) * i_conv_desc->ofm_block + i_ofm) * l_conv_kernel_config.datatype_size,
                                           l_conv_kernel_config.vector_name,
                                           l_conv_kernel_config.vector_reg_count-ofw_reg_block_factor-1 /* weight */,
                                           l_conv_kernel_config.vector_reg_count-oi_-1 /* input loaded */);
            }
          }
          for(oi_=0; oi_<ofw_reg_block_factor; oi_++) {
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                    l_conv_kernel_config.instruction_set,
                                    l_conv_kernel_config.vmove_instruction,
                                    l_gp_reg_mapping.gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    (oi+oi_+ki)*l_conv_kernel_config.vector_length * l_conv_kernel_config.datatype_size,
                                    l_conv_kernel_config.vector_name,
                                    l_conv_kernel_config.vector_reg_count-oi_-1 , 0, 1 /*store*/);
          }
        }
      } else { /* do not unroll kw */
          fprintf(stderr, "ERROR: JIT CODE NOT YET FULLY GENERATED\n");
        /* TODO: Finish this */

        /* header of kw loop */
        libxsmm_generator_convolution_header_kw_loop(  io_generated_code, &l_loop_label_tracker,
                                               &l_conv_kernel_config, l_gp_reg_mapping.gp_reg_kw_loop );

        /* Add code here */

        /* footer of kw loop */
        libxsmm_generator_convolution_footer_kw_loop(  io_generated_code, &l_loop_label_tracker,
                                               &l_conv_kernel_config, l_gp_reg_mapping.gp_reg_kw_loop, i_conv_desc->kw );
      }
    }
  } else { /* Completely unroll and no latency hiding scenario */
    fprintf(stderr, "ERROR: JIT CODE NOT YET FULLY GENERATED\n");
  }
#endif

  /* close asm */
  libxsmm_x86_instruction_close_stream_convolution( io_generated_code, i_arch );
}

LIBXSMM_INTERNAL_API_DEFINITION
void libxsmm_generator_convolution_backward_avx512_ofmloop( libxsmm_generated_code*                           io_generated_code,
                                                           const libxsmm_convolution_backward_gp_reg_mapping* i_gp_reg_mapping,
                                                           const libxsmm_convolution_kernel_config*          i_conv_kernel_config,
                                                           const libxsmm_convolution_backward_descriptor*     i_conv_desc,
                                                           const unsigned int                                i_kw_unroll)
{
  libxsmm_generator_convolution_backward_avx512_ofmloop_sfma( io_generated_code, i_gp_reg_mapping,
                                                             i_conv_kernel_config, i_conv_desc, i_kw_unroll);
}

LIBXSMM_INTERNAL_API_DEFINITION
void libxsmm_generator_convolution_backward_avx512_ofmloop_peeled( libxsmm_generated_code*                           io_generated_code,
                                                           const libxsmm_convolution_backward_gp_reg_mapping* i_gp_reg_mapping,
                                                           const libxsmm_convolution_kernel_config*          i_conv_kernel_config,
                                                           const libxsmm_convolution_backward_descriptor*     i_conv_desc,
                                                           const unsigned int                                i_kw_unroll,
                                                           const unsigned int                                i_kh_unroll)
{
  libxsmm_generator_convolution_backward_avx512_ofmloop_sfma_peeled( io_generated_code, i_gp_reg_mapping,
                                                             i_conv_kernel_config, i_conv_desc, i_kw_unroll, i_kh_unroll);
}

LIBXSMM_INTERNAL_API_DEFINITION
void libxsmm_generator_convolution_backward_avx512_init_output_strides( libxsmm_generated_code*                           io_generated_code,
                                                                      const libxsmm_convolution_backward_gp_reg_mapping* i_gp_reg_mapping,
                                                                      const libxsmm_convolution_kernel_config*          i_conv_kernel_config,
                                                                      const libxsmm_convolution_backward_descriptor*     i_conv_desc ) {
  /* Intialize helper registers for SIB addressing */
  /* helper 0: Index register holding ldb*datatype_size */
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_mov_instruction,
                                   i_gp_reg_mapping->gp_reg_help_0, i_conv_kernel_config->datatype_size /* * i_conv_desc->stride_w*/ * i_conv_desc->ofm_block );
  /* helper 1: Index register holding 3*ldb*datatype_size */
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_mov_instruction,
                                   i_gp_reg_mapping->gp_reg_help_1, i_conv_kernel_config->datatype_size /** i_conv_desc->stride_w*/ * i_conv_desc->ofm_block * 3 );
  /* helper 2: Index register holding 5*ldb*datatype_size */
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_mov_instruction,
                                   i_gp_reg_mapping->gp_reg_help_2, i_conv_kernel_config->datatype_size * /*i_conv_desc->stride_w **/ i_conv_desc->ofm_block * 5 );
  /* helper 3: Index register holding 7*ldb*datatype_size */
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_mov_instruction,
                                   i_gp_reg_mapping->gp_reg_help_3, i_conv_kernel_config->datatype_size * /*i_conv_desc->stride_w **/ i_conv_desc->ofm_block * 7 );

  /* helper 4: B + 9*ldb, additional base address
     helper 5: B + 18*ldb, additional base adrress */
  if ( i_conv_desc->ofw_rb > 9 ) {
    libxsmm_x86_instruction_alu_reg( io_generated_code, i_conv_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_output, i_gp_reg_mapping->gp_reg_help_4);
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_help_4,  9 * i_conv_kernel_config->datatype_size * /*i_conv_desc->stride_w **/ i_conv_desc->ofm_block );
  }
  if ( i_conv_desc->ofw_rb > 18 ) {
    libxsmm_x86_instruction_alu_reg( io_generated_code, i_conv_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_output, i_gp_reg_mapping->gp_reg_help_5);
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_help_5, 18 *  i_conv_kernel_config->datatype_size * /*i_conv_desc->stride_w **/ i_conv_desc->ofm_block );
  }
  if ( i_conv_desc->ofw_rb > 27 ) {
    libxsmm_x86_instruction_alu_reg( io_generated_code, i_conv_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_output, i_gp_reg_mapping->gp_reg_help_6);
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_help_6, 27 *  i_conv_kernel_config->datatype_size * /*i_conv_desc->stride_w **/ i_conv_desc->ofm_block );
  }
}

LIBXSMM_INTERNAL_API_DEFINITION
void libxsmm_generator_convolution_backward_avx512_ofmloop_sfma( libxsmm_generated_code*                           io_generated_code,
                                                                const libxsmm_convolution_backward_gp_reg_mapping* i_gp_reg_mapping,
                                                                const libxsmm_convolution_kernel_config*          i_conv_kernel_config,
                                                                const libxsmm_convolution_backward_descriptor*     i_conv_desc,
                                                                const unsigned int                                i_kw_unroll ) {
  unsigned int l_n;
  unsigned int l_k_1, l_k_2;
  unsigned int l_output_reg;
  unsigned int l_output_idx;
  unsigned int l_scale;
  unsigned int l_disp;
  unsigned int l_displacement_k = 0;
  unsigned int l_k_updates = 0;
  unsigned int l_w;
  unsigned int reg_count = 0;
  unsigned int l_weight_updates = 0;
  unsigned int j,k;
  unsigned int l_k = 0;
#if 0
  unsigned int l_j=0;
#endif
/* determine the number of registers needed for an ofm block */
  const unsigned int l_reg_per_block = i_conv_desc->ofm_block / i_conv_kernel_config->vector_length;
  /* start register of accumulator */
  const unsigned int l_vec_reg_acc_start = i_conv_kernel_config->vector_reg_count - (i_conv_desc->ofw_rb * l_reg_per_block);
#if 0
  const unsigned int oi_prefetch = (i_conv_desc->ofw / i_conv_desc->ofw_rb) > 1 ? 1 : 0;
#endif
  /* TODO: WHEN ofm_block is greater than 128, please move this inside the for-loop */
  libxsmm_generator_convolution_backward_avx512_init_output_strides( io_generated_code, i_gp_reg_mapping, i_conv_kernel_config, i_conv_desc );

  for ( l_k_1 = 0; l_k_1 < i_kw_unroll ; l_k_1++ ) {
    if(l_k_1 == 0) {
      /* load all inputs */
    reg_count = 0;
    for ( j = 0; j < i_conv_desc->ofw_rb; j++ ) {
      for ( k = 0; k < l_reg_per_block ; k++, reg_count++ ) {
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                    i_conv_kernel_config->instruction_set,
                                    i_conv_kernel_config->vmove_instruction,
                                    i_gp_reg_mapping->gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    (reg_count+l_k_1)* /*i_conv_desc->kw**/i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size,
                                    i_conv_kernel_config->vector_name,
                                    l_vec_reg_acc_start + reg_count , 0, 0 /*load*/);

  /* This is prefetching for next ij loop assuming that oi loop is completely unrolled*/
  /* Ideally, if oi is loop is not completely unrolled, you need to prefetch for next chunk of oi loop see below */
#ifdef ENABLE_INPUT_PREFETCH
  if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     i_gp_reg_mapping->gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((reg_count)* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size));
  }
#endif
#if 0
  if((oi_prefetch == 1 ) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     i_gp_reg_mapping->gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((reg_count+l_k_1)* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size) + (i_conv_desc->ofw_rb  * i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size));
  }
#endif
      }
    }
    } else {
    /* load input */
    libxsmm_x86_instruction_vec_move( io_generated_code,
                                    i_conv_kernel_config->instruction_set,
                                    i_conv_kernel_config->vmove_instruction,
                                    i_gp_reg_mapping->gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    ((i_conv_desc->ofw_rb * l_reg_per_block)-1+l_k_1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size,
                                    i_conv_kernel_config->vector_name,
                                    i_conv_kernel_config->vector_reg_count - (i_conv_desc->ofw_rb* l_reg_per_block) + ((l_k_1-1)%(i_conv_desc->ofw_rb * l_reg_per_block)),
                                      0, 0 /*load*/);
#if 1
#ifdef ENABLE_INPUT_PREFETCH
  if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     i_gp_reg_mapping->gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((i_conv_desc->ofw_rb * l_reg_per_block)-1+l_k_1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size);
  }
#endif
#endif
    }
#ifdef OLD_PREFETCH_INPUT_L1
    if(l_k_1 < i_kw_unroll-1) {
  if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     i_gp_reg_mapping->gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     (l_k_1)* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size + (i_conv_desc->ofw_rb  * i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size));
  }
  }else {
    reg_count = 0;
    if((oi_prefetch == 1 ) && (i_conv_desc->ofw_unroll == 1) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
    for ( j = 0; j < i_conv_desc->ofw_rb; j++ ) {
      for ( k = 0; k < l_reg_per_block ; k++, reg_count++ ) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     i_gp_reg_mapping->gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((reg_count)* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size) + (i_conv_desc->ofw_rb  * i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size));
  }
      }
    }
  }
#endif
#if 0
  if(((i_kw_unroll > 1) && (l_k_1 < i_kw_unroll-1)) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
    libxsmm_x86_instruction_prefetch( io_generated_code,
                                          LIBXSMM_X86_INSTR_PREFETCHT0,
                                          /*i_conv_kernel_config->prefetch_instruction,*/
                                          /*i_gp_reg_mapping->gp_reg_input_pf,*/
                                          i_gp_reg_mapping->gp_reg_input,
                                          LIBXSMM_X86_GP_REG_UNDEF,
                                          0,
                                    ((i_conv_desc->ofw_rb * l_reg_per_block)-1+1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size
                                          );
  }
#endif
  l_displacement_k = 0;
  l_k_updates = 0;
  l_w=0;
  for ( l_k_2 = 0; l_k_2 < i_conv_desc->ofm_block ; l_k_2++, l_k++) {


#if 0
    /* the first load in next ki and others are pipelined */
    if((l_k_2 == i_conv_desc->ofm_block-1) && (l_k_1 < i_kw_unroll-1)) {
      /* L1_prefetch(tr_wt[ofm1][ifm1][kh-kj-1][ki+1][0]) */
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L1) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                          LIBXSMM_X86_INSTR_PREFETCHT0,
                                          /*i_conv_kernel_config->prefetch_instruction,*/
                                          i_gp_reg_mapping->gp_reg_weight_pf,
                                          LIBXSMM_X86_GP_REG_UNDEF,
                                          0,
                                          (l_k+1)*(i_conv_desc->ofm_block)*(i_conv_kernel_config->datatype_size));
    }
    }
#endif
#if 0
    /* Every iteration try to bring in a new cache line for output which will be needed after 16 iteration */
    if(l_k_1 == i_kw_unroll-1) && (l_k_2 < i_conv_desc->ofm_block)) {
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                          LIBXSMM_X86_INSTR_PREFETCHT0,
                                          /*i_conv_kernel_config->prefetch_instruction,*/
                                          i_gp_reg_mapping->gp_reg_output_pf,
                                          LIBXSMM_X86_GP_REG_UNDEF,
                                          0,
                                          (i_conv_desc->ofm_block + l_k) * i_conv_kernel_config->datatype_size * i_conv_desc->ofm_block );

      }
    }

    if(l_k_2 < i_conv_desc->ofm_block-1) {

    } else if((l_k_2 == i_conv_desc->ofm_block-1) && (l_k_1 < i_kw_unroll-1)) {
#if 0
      /* L1_prefetch(tr_wt[ofm1][ifm1][kh-kj-1][ki+1][0]) */
      if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L1) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                          LIBXSMM_X86_INSTR_PREFETCHT0,
                                          /*i_conv_kernel_config->prefetch_instruction,*/
                                          i_gp_reg_mapping->gp_reg_weight_pf,
                                          LIBXSMM_X86_GP_REG_UNDEF,
                                          0,
                                          (l_k+1)*(i_conv_desc->ofm_block)*(i_conv_kernel_config->datatype_size));
#endif
      }
    } else if ((l_k_2 == i_conv_desc->ofm_block-1) && (l_k_1 == i_kw_unroll-1)) {
        /* TODO: FIXME */
        /*L1_prefetch(next block of tiled oi for tr_wt);*/
        /*L2 (L1?)_prefetch(next block of tiled oi for del_output);*/ /* in theory you should load ur_i number of them */

    }
#endif
#if 0
    /* TODO unless and otherwise ofm_block> 128 We will not run into this scenario */
    /* advance b pointer if needed */
    if ( (l_k_2 > 0) && (l_k_2%8 == 0) ) {
      libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_add_instruction,
                                       i_gp_reg_mapping->gp_reg_output, 8* i_conv_desc->ofm_block * i_conv_kernel_config->datatype_size );
      /* advance the second base pointer only if it's needed */
      if ( i_conv_desc->ofw_rb > 8 ) {
        libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_add_instruction,
                                         i_gp_reg_mapping->gp_reg_help_4, 8* i_conv_desc->ofm_block *i_conv_kernel_config->datatype_size );
      }
      /* advance the third base pointer only if it's needed */
      if ( i_conv_desc->ofw_rb > 17 ) {
        libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_add_instruction,
                                         i_gp_reg_mapping->gp_reg_help_5, 8* i_conv_desc->ofm_block *i_conv_kernel_config->datatype_size );
      }
      /* advance the fourth base pointer only if it's needed */
      if ( i_conv_desc->ofw_rb > 26 ) {
        libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_add_instruction,
                                         i_gp_reg_mapping->gp_reg_help_6, 8*i_conv_desc->ofm_block * i_conv_kernel_config->datatype_size );
      }

      l_displacement_k = 0;
      l_k_updates++;
    }
#endif
#if 0
        if ((l_k > 0) && ((l_k % (2*i_conv_desc->ofm_block)) == 0)) {
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_weight, 2*i_conv_desc->ofm_block* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size  );

          l_weight_updates++;
        }
#endif

    if ( l_k == 0 ) {
      /* load weights */
      libxsmm_x86_instruction_vec_move( io_generated_code,
                                        i_conv_kernel_config->instruction_set,
                                        i_conv_kernel_config->vmove_instruction,
                                        i_gp_reg_mapping->gp_reg_weight,
                                        LIBXSMM_X86_GP_REG_UNDEF, 0,
                                        (l_k)* /*(i_conv_desc->ofm_block)*/ (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size) - (l_weight_updates * 2 * i_conv_desc->ofm_block * i_conv_kernel_config->vector_length*i_conv_kernel_config->datatype_size),
                                        i_conv_kernel_config->vector_name, 0,
                                        0, 0 );
#ifdef NO_PEELED_PREFETCH_WEIGHT
      /* bring in for next kj to cache */
      if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2)) {
        if ((i_kw_unroll > 1) /*&& (l_k_1 == (i_kw_unroll -1))*/) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0-i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  + l_k * (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size) );

        }
      }
#endif

      if ( i_conv_desc->ofm_block *i_kw_unroll > 1 ) {
        for ( l_w = 1; l_w < 4; l_w++ ) {
        /* second weight loaded in first iteration, in case of large blockings -> hiding L1 latencies */
        libxsmm_x86_instruction_vec_move( io_generated_code,
                                          i_conv_kernel_config->instruction_set,
                                          i_conv_kernel_config->vmove_instruction,
                                          i_gp_reg_mapping->gp_reg_weight,
                                          LIBXSMM_X86_GP_REG_UNDEF, 0,
                                          (l_k + l_w)* /*(i_conv_desc->ofm_block) */(i_conv_kernel_config->vector_length) *(i_conv_kernel_config->datatype_size) - (l_weight_updates * 2 * i_conv_desc->ofm_block * i_conv_kernel_config->vector_length*i_conv_kernel_config->datatype_size),
                                          i_conv_kernel_config->vector_name, l_w,
                                          0, 0 );
#ifdef NO_PEELED_PREFETCH_WEIGHT
      if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2)) {
        if ((i_kw_unroll > 1) /*&& (l_k_1 == (i_kw_unroll -1))*/) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0-i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size + (l_k + l_w) * (i_conv_kernel_config->vector_length) *(i_conv_kernel_config->datatype_size) );
        }
      }
#endif
        }
      }
    } else if ( l_k < ((i_conv_desc->ofm_block * i_kw_unroll) - 3) /*l_k_2 < (i_conv_desc->ofm_block -3) */) {
      /* pipelined load of weight, one k iteration ahead */
      libxsmm_x86_instruction_vec_move( io_generated_code,
                                        i_conv_kernel_config->instruction_set,
                                        i_conv_kernel_config->vmove_instruction,
                                        i_gp_reg_mapping->gp_reg_weight,
                                        LIBXSMM_X86_GP_REG_UNDEF, 0,
                                        (l_k + 3)* /*(i_conv_desc->ofm_block)*/ (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size) -  (l_weight_updates * 2 * i_conv_desc->ofm_block* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size),
                                        i_conv_kernel_config->vector_name, (l_k+3)%4,
                                        0, 0 );
#ifdef NO_PEELED_PREFETCH_WEIGHT
      if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2)) {
        if ((i_kw_unroll > 1) /*&& (l_k_1 == (i_kw_unroll -1))*/) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0-i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size + (l_k + 3) * (i_conv_kernel_config->vector_length) *(i_conv_kernel_config->datatype_size) );
        }
      }
#endif
    }
    /* compute vectorwidth (A) * column broadcast (B) */
    for ( l_n = 0; l_n < i_conv_desc->ofw_rb; l_n++) {
      /* determining base, idx and scale values */
      /* default values */
      l_output_reg = i_gp_reg_mapping->gp_reg_output;
      l_output_idx = LIBXSMM_X86_GP_REG_UNDEF;
      l_scale = 0;
      l_disp = l_displacement_k*i_conv_kernel_config->datatype_size;

      /* select the base register */
      if ( l_n > 26 ) {
        l_output_reg = i_gp_reg_mapping->gp_reg_help_6;
      } else if ( l_n > 17 ) {
        l_output_reg = i_gp_reg_mapping->gp_reg_help_5;
      } else if ( l_n > 8 ) {
        l_output_reg = i_gp_reg_mapping->gp_reg_help_4;
      } else {
        l_output_reg = i_gp_reg_mapping->gp_reg_output;
      }
      if ( l_n % 9 == 0 ) {
        l_output_idx = LIBXSMM_X86_GP_REG_UNDEF;
        l_scale = 0;
      } else if ( l_n % 9 == 1 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_0;
        l_scale = 1;
      } else if ( l_n % 9 == 2 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_0;
        l_scale = 2;
      } else if ( l_n % 9 == 3 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_1;
        l_scale = 1;
      } else if ( l_n % 9 == 4 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_0;
        l_scale = 4;
      } else if ( l_n % 9 == 5 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_2;
        l_scale = 1;
      } else if ( l_n % 9 == 6 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_1;
        l_scale = 2;
      } else if ( l_n % 9 == 7 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_3;
        l_scale = 1;
      } else if ( l_n % 9 == 8 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_0;
        l_scale = 8;
      } else {
        /* shouldn't happen.... */
      }

      libxsmm_x86_instruction_vec_compute_mem( io_generated_code,
                                               i_conv_kernel_config->instruction_set,
                                               i_conv_kernel_config->vfma_instruction,
                                               1,
                                               l_output_reg,
                                               l_output_idx,
                                               l_scale,
                                               l_disp,
                                               i_conv_kernel_config->vector_name,
                                               (l_k%4),
                                               i_conv_kernel_config->vector_reg_count - (i_conv_desc->ofw_rb* l_reg_per_block) + ((l_n+l_k_1)%(i_conv_desc->ofw_rb * l_reg_per_block)));

#ifdef ENABLE_OUTPUT_PREFETCH
      /* TODO POSSIBLE BUG: if kw is not unrolled and you run over the last iteration of l_k_1, you may access unwanted memory and the prefetches are junk... do not help */
      if ( (l_n == 3) && (l_k < (/*(i_conv_desc->prefetch_output_ahead == 1) ? (i_conv_desc->kh *  i_conv_desc->ofw_padded) :*/ (i_conv_desc->ofw /*_padded*/))) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) ) {
        libxsmm_x86_instruction_prefetch( io_generated_code,
                                          LIBXSMM_X86_INSTR_PREFETCHT0,
                                          i_gp_reg_mapping->gp_reg_output_pf,
                                          LIBXSMM_X86_GP_REG_UNDEF,
                                          0,
                                          (l_k * i_conv_kernel_config->datatype_size * i_conv_desc->ofm_block) );
      }
#endif
#ifdef ENABLE_WEIGHT_PREFETCH
      if ( (l_n == 8) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) ) {
        libxsmm_x86_instruction_prefetch( io_generated_code,
                                          /*i_conv_kernel_config->prefetch_instruction,*/
                                          LIBXSMM_X86_INSTR_PREFETCHT0 /*i_conv_kernel_config->prefetch_instruction*/,
                                          i_gp_reg_mapping->gp_reg_weight_pf,
                                          LIBXSMM_X86_GP_REG_UNDEF, 0,
                                          /* TODO POSSIBLE BUG: if kw is not unrolled and you run over the last iteration of l_k_1, you may access unwanted memory and the prefetches are junk... do not help */
                                          (l_k)* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size);
      }
#endif


#ifdef OLD_PREFETCH_OUTPUT_L1
  if((oi_prefetch == 1 ) && (l_k_1 == (i_kw_unroll-1)) && (l_k_2 == (i_conv_desc->ofm_block -3)) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     i_gp_reg_mapping->gp_reg_output_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     (i_conv_desc->ofw_rb + l_n)*  i_conv_desc->ofm_block * i_conv_kernel_config->datatype_size  );
  }
#endif

    }
    l_displacement_k++;
  }
#if 1
  /* we have to make sure that we are reseting the pointer to its original value in case a full unroll */
  if ( l_k_updates > 0 ) {
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_output, 8*l_k_updates* i_conv_desc->ofm_block * i_conv_kernel_config->datatype_size  );
  }
#endif

    reg_count = 0;
    if(l_k_1 == i_kw_unroll-1) {
    reg_count = 0;
        /* store inputs */
       for (j = 0; j < i_conv_desc->ofw_rb; j++ ) {
      for (k = 0; k < l_reg_per_block ; k++, reg_count++ ) {
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                    i_conv_kernel_config->instruction_set,
                                    i_conv_kernel_config->vmove_instruction,
                                    i_gp_reg_mapping->gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    (reg_count+l_k_1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size,
                                    i_conv_kernel_config->vector_name,
                                    l_vec_reg_acc_start + ((reg_count+l_k_1) % (i_conv_desc->ofw_rb * l_reg_per_block)) , 0, 1 /*store*/);
      }
    }

    } else {
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                    i_conv_kernel_config->instruction_set,
                                    i_conv_kernel_config->vmove_instruction,
                                    i_gp_reg_mapping->gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    (reg_count+l_k_1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size,
                                    i_conv_kernel_config->vector_name,
                                    l_vec_reg_acc_start + ((reg_count+l_k_1) % (i_conv_desc->ofw_rb * l_reg_per_block)) , 0, 1 /*store*/);

    }
    if(i_kw_unroll > 1) {
#if 0
    if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) {
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_input_pf, i_conv_kernel_config->vector_length*i_conv_kernel_config->datatype_size  );
    }
#endif
    }
  }
  /* we have to make sure that we are reseting the pointer to its original value in case a full unroll */
  if ( l_weight_updates > 0 ) {
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_weight, l_weight_updates * 2 * i_conv_desc->ofm_block* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size  );
  }

}

/* Peeled version */
LIBXSMM_INTERNAL_API_DEFINITION
void libxsmm_generator_convolution_backward_avx512_ofmloop_sfma_peeled( libxsmm_generated_code*                           io_generated_code,
                                                                const libxsmm_convolution_backward_gp_reg_mapping* i_gp_reg_mapping,
                                                                const libxsmm_convolution_kernel_config*          i_conv_kernel_config,
                                                                const libxsmm_convolution_backward_descriptor*     i_conv_desc,
                                                                const unsigned int                                i_kw_unroll,
                                                                const unsigned int                                i_kh_unroll ) {
  unsigned int l_n;
  unsigned int l_k_1, l_k_2, l_k_3;
  unsigned int l_output_reg;
  unsigned int l_output_idx;
  unsigned int l_scale;
  unsigned int l_disp;
  unsigned int l_displacement_k = 0;
  unsigned int l_w;
  unsigned int reg_count = 0;
  unsigned int l_k = 0;
  unsigned int j,k;
  unsigned int weight_counter;
#if 0
  unsigned int l_weight_updates = 0;
  unsigned int l_k_updates = 0;
  unsigned int l_k = 0, l_j=0;
#endif
/* determine the number of registers needed for an ofm block */
  const unsigned int l_reg_per_block = i_conv_desc->ofm_block / i_conv_kernel_config->vector_length;
  /* start register of accumulator */
  const unsigned int l_vec_reg_acc_start = i_conv_kernel_config->vector_reg_count - (i_conv_desc->ofw_rb * l_reg_per_block);
#if 0
  const unsigned int oi_prefetch = (i_conv_desc->ofw / i_conv_desc->ofw_rb) > 1 ? 1 : 0;
#endif
#define OPT_LOAD_STORE
  for ( l_k_1 = 0; l_k_1 < i_kw_unroll ; l_k_1++ ) {

    libxsmm_generator_convolution_backward_avx512_init_output_strides( io_generated_code, i_gp_reg_mapping, i_conv_kernel_config, i_conv_desc );

#ifdef OPT_LOAD_STORE
    if(l_k_1 == 0) {
      /* load all inputs */
    reg_count = 0;
    for ( j = 0; j < i_conv_desc->ofw_rb; j++ ) {
      for ( k = 0; k < l_reg_per_block ; k++, reg_count++ ) {
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                    i_conv_kernel_config->instruction_set,
                                    i_conv_kernel_config->vmove_instruction,
                                    i_gp_reg_mapping->gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    (reg_count+l_k_1)* /*i_conv_desc->kw**/i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size,
                                    i_conv_kernel_config->vector_name,
                                    l_vec_reg_acc_start + reg_count , 0, 0 /*load*/);
#ifdef ENABLE_INPUT_PREFETCH
  /* This is prefetching for next ij loop assuming that oi loop is completely unrolled*/
  /* Ideally, if oi is loop is not completely unrolled, you need to prefetch for next chunk of oi loop see below */
  if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     i_gp_reg_mapping->gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((reg_count)* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size));
  }
#endif
#ifdef OLD_PREFETCH_INPUT_L1
  /* prefetch for next oi independent of unroll and non-unroll*/
  if((oi_prefetch == 1 ) && (i_conv_desc->ofw_unroll == 1) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*LIBXSMM_X86_INSTR_PREFETCHT0,*/
                                     i_conv_kernel_config->prefetch_instruction,
                                     i_gp_reg_mapping->gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((reg_count+i_conv_desc->ofw_rb)* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size));
  }
#endif
#if 0
  /* prefetch for next ki if ki was not unrolled -- might be wrong when unrolled version has the last iteration TODO*/
  if(((i_conv_desc->unroll_kw == 0) && (i_conv_desc->kw != 1)) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((reg_count+1)* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size));
  }
#endif
      }
    }
    } else {
    /* load input */
      libxsmm_x86_instruction_vec_move( io_generated_code,
                                    i_conv_kernel_config->instruction_set,
                                    i_conv_kernel_config->vmove_instruction,
                                    i_gp_reg_mapping->gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    ((i_conv_desc->ofw_rb * l_reg_per_block)-1+l_k_1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size,
                                    i_conv_kernel_config->vector_name,
                                    i_conv_kernel_config->vector_reg_count - (i_conv_desc->ofw_rb* l_reg_per_block) + ((l_k_1-1)%(i_conv_desc->ofw_rb * l_reg_per_block)),
                                      0, 0 /*load*/);
#if 1
#ifdef ENABLE_INPUT_PREFETCH
  /* This is prefetching for next ij loop assuming that oi loop is completely unrolled*/
  /* Ideally, if oi is loop is not completely unrolled, you need to prefetch for next chunk of oi loop see below */
  if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     i_gp_reg_mapping->gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((i_conv_desc->ofw_rb * l_reg_per_block)-1+l_k_1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size);
  }
#endif
#endif

#ifdef OLD_PREFETCH_INPUT_L1
      /* prefetch when kw is unrolled, only the next load after optimization; what to do about last iteration of i_kw_unroll */
      if(((i_kw_unroll > 1)) && (l_k_1 < i_kw_unroll -1) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                    ((i_conv_desc->ofw_rb * l_reg_per_block)-1+l_k_1+1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size);
      }
#endif
    }
#else /* NO_OPT_LOAD_STORE */
    reg_count = 0;
    for ( j = 0; j < i_conv_desc->ofw_rb; j++ ) {
      for ( k = 0; k < l_reg_per_block ; k++, reg_count++ ) {
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                    i_conv_kernel_config->instruction_set,
                                    i_conv_kernel_config->vmove_instruction,
                                    i_gp_reg_mapping->gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    (reg_count+l_k_1)* /*i_conv_desc->kw**/i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size,
                                    i_conv_kernel_config->vector_name,
                                    l_vec_reg_acc_start + reg_count , 0, 0 /*load*/);
      }
    }
#endif
#ifdef OLD_PREFETCH_INPUT_L1
  if ((i_conv_desc->unroll_kw == 0) && (i_conv_desc->kw > 1) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_input_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((i_conv_desc->ofw_rb * l_reg_per_block)-1+1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size);
  }
#endif
#if 0
//#ifdef ENABLE_INPUT_PREFETCH
      /* prefetch when kw is unrolled, only the next load after optimization; what to do about last iteration of i_kw_unroll */
      if(((i_kw_unroll > 1)) && (l_k_1 < i_kw_unroll -1) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_INPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_input,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     ((i_conv_desc->ofw_rb * l_reg_per_block)-1+l_k_1+1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size);
      }
//#endif
#endif

  l_k = 0;
  weight_counter = 0;
  for ( l_k_3 = 0; l_k_3 < i_kh_unroll ; l_k_3++) {

  l_displacement_k = 0;
  l_w=0;
#if 0
  l_k_updates = 0;
#endif
  for ( l_k_2 = 0; l_k_2 < i_conv_desc->ofm_block ; l_k_2++, l_k++) {

    if ( /*l_k_2 == 0 */ l_k == 0) {
      /* load weights */
      libxsmm_x86_instruction_vec_move( io_generated_code,
                                        i_conv_kernel_config->instruction_set,
                                        i_conv_kernel_config->vmove_instruction,
                                        i_gp_reg_mapping->gp_reg_weight,
                                        LIBXSMM_X86_GP_REG_UNDEF, 0,
                                        ((/*l_k_1 +*/ /*l_k_2*/ l_k)* /*(i_conv_desc->ofm_block)*/ (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size)) /*- (l_weight_updates * 2 * i_conv_desc->ofm_block * i_conv_kernel_config->vector_length*i_conv_kernel_config->datatype_size)*/
                                        - (l_k_3 * i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ),
                                        i_conv_kernel_config->vector_name, 0,
                                        0, 0 );
#ifdef OLD_PREFETCH_WEIGHT
      if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2)) {
        if((l_k_3 < (i_conv_desc->kh-1))) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0-(i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ) + ((l_k_2)* /*(i_conv_desc->ofm_block)*/ (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size)));
        } else if ((l_k_3 == (i_conv_desc->kh-1)) && (i_kw_unroll > 1) && (l_k_1 < (i_kw_unroll -1))) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0+((i_conv_desc->kh-1) * i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ) + ((l_k_1+1)* (i_conv_desc->ofm_block) *  (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size)));
        } else if((l_k_3 == (i_conv_desc->kh-1)) && (i_kw_unroll > 1)  && (l_k_1 == (i_kw_unroll -1))) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0+((i_conv_desc->kh-1) * i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ) - ((i_kw_unroll-1)* (i_conv_desc->ofm_block) *  (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size)));

        }
      }
#endif
      if ( i_conv_desc->ofm_block *i_kh_unroll > 1 ) {
         for ( l_w = 1; l_w < 4; l_w++ ) {
          /* second weight loaded in first iteration, in case of large blockings -> hiding L1 latencies */
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                          i_conv_kernel_config->instruction_set,
                                          i_conv_kernel_config->vmove_instruction,
                                          i_gp_reg_mapping->gp_reg_weight,
                                          LIBXSMM_X86_GP_REG_UNDEF, 0,
                                          ((/*l_k_1 +*/ l_k + l_w)* /*(i_conv_desc->ofm_block) */(i_conv_kernel_config->vector_length) *(i_conv_kernel_config->datatype_size) /*- (l_weight_updates * 2 * i_conv_desc->ofm_block * i_conv_kernel_config->vector_length*i_conv_kernel_config->datatype_size)*/)
                                          - (l_k_3 * i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ),
                                          i_conv_kernel_config->vector_name, l_w,
                                          0, 0 );
#ifdef OLD_PREFETCH_WEIGHT
          if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2)) {

              /* Assume pipeline does not work */
              libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     (/*l_k_1 +*/ l_k_2 + l_w)* /*(i_conv_desc->ofm_block) */(i_conv_kernel_config->vector_length) *(i_conv_kernel_config->datatype_size) /*- (l_weight_updates * 2 * i_conv_desc->ofm_block * i_conv_kernel_config->vector_length*i_conv_kernel_config->datatype_size)*/);


            if((l_k_3 < (i_conv_desc->kh-1))) {
              libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0-(i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ) + ((l_k_2 + l_w)* /*(i_conv_desc->ofm_block)*/ (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size)));
            } else if ((l_k_3 == (i_conv_desc->kh-1)) && (i_kw_unroll > 1)  && (l_k_1 < (i_kw_unroll -1))) {
              libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0+((i_conv_desc->kh-1) * i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ) + ((l_k_2 + l_w + ((l_k_1+1) * i_conv_desc->ofm_block))* (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size)));
            } else if((l_k_3 == (i_conv_desc->kh-1)) && (i_kw_unroll > 1)  && (l_k_1 == (i_kw_unroll -1))) {
              libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0+((i_conv_desc->kh-1) * i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ) - ((l_k_2 + l_w + (i_kw_unroll-1)* i_conv_desc->ofm_block) *  (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size)));
            }
          }
#endif
        }
        weight_counter += 3;
      }
    } else if ((l_k < ((i_conv_desc->ofm_block * i_kh_unroll) - 3)) && (l_k_2 >= (i_conv_desc->ofm_block -3)) && (l_k_2 < i_conv_desc->ofm_block) ) {
      libxsmm_x86_instruction_vec_move( io_generated_code,
                                        i_conv_kernel_config->instruction_set,
                                        i_conv_kernel_config->vmove_instruction,
                                        i_gp_reg_mapping->gp_reg_weight,
                                        LIBXSMM_X86_GP_REG_UNDEF, 0,
                                        ((/*l_k_1 +*/ weight_counter)* /*(i_conv_desc->ofm_block)*/ (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size) /*-  (l_weight_updates * 2 * i_conv_desc->ofm_block* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size)*/)
                                        - ((l_k_3 + 1) * i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ),
                                        i_conv_kernel_config->vector_name, (l_k+3)%4,
                                        0, 0 );
    } else if ( l_k < ((i_conv_desc->ofm_block * i_kh_unroll) - 3) /*l_k_2 < (i_conv_desc->ofm_block -3)*/ ) {
      /* pipelined load of weight, one k iteration ahead */
      libxsmm_x86_instruction_vec_move( io_generated_code,
                                        i_conv_kernel_config->instruction_set,
                                        i_conv_kernel_config->vmove_instruction,
                                        i_gp_reg_mapping->gp_reg_weight,
                                        LIBXSMM_X86_GP_REG_UNDEF, 0,
                                        ((/*l_k_1 +*/ weight_counter /*+ 3*/)* /*(i_conv_desc->ofm_block)*/ (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size) /*-  (l_weight_updates * 2 * i_conv_desc->ofm_block* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size)*/)
                                        - (l_k_3 * i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ),
                                        i_conv_kernel_config->vector_name, (l_k+3)%4,
                                        0, 0 );
#ifdef OLD_PREFETCH_WEIGHT
      if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2)) {
              /* Assume pipeline does not work */
              libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     (/*l_k_1 +*/ l_k_2 + 3)* /*(i_conv_desc->ofm_block)*/ (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size) /*-  (l_weight_updates * 2 * i_conv_desc->ofm_block* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size)*/);
        if((l_k_3 < (i_conv_desc->kh-1))) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0-(i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ) + ((l_k_2 + 3)* /*(i_conv_desc->ofm_block)*/ (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size)));
        } else if ((l_k_3 == (i_conv_desc->kh-1)) && (i_kw_unroll > 1)  && (l_k_1 < (i_kw_unroll -1))) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0+((i_conv_desc->kh-1) * i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ) + ((l_k_2 + 3 + ((l_k_1+1) * i_conv_desc->ofm_block))* (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size)));
        } else if((l_k_3 == (i_conv_desc->kh-1)) && (i_kw_unroll > 1)  && (l_k_1 == (i_kw_unroll -1))) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
                                     /*i_conv_kernel_config->prefetch_instruction,*/
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0+((i_conv_desc->kh-1) * i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ) - ((l_k_2 + 3 + (i_kw_unroll-1)* i_conv_desc->ofm_block) *  (i_conv_kernel_config->vector_length)*(i_conv_kernel_config->datatype_size)));
        }
      }
#endif

#if 0
      /*prefetch for next iteration of l_k_3 -- only one load -- keep in mind that weight decreases with kj*/
      if((l_k_3 < (i_conv_desc->kh-1)) && (l_k_2 == (i_conv_desc->ofm_block -4)) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L1)) {
        libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     0-(i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ));
      }
#endif
    }

    if(weight_counter >= (i_conv_desc->ofm_block-1)){
      weight_counter = 0;
    } else weight_counter++;

    /* compute vectorwidth (A) * column broadcast (B) */
    for ( l_n = 0; l_n < i_conv_desc->ofw_rb; l_n++) {
      /* determining base, idx and scale values */
      /* default values */
      l_output_reg = i_gp_reg_mapping->gp_reg_output;
      l_output_idx = LIBXSMM_X86_GP_REG_UNDEF;
      l_scale = 0;
      l_disp = l_displacement_k*i_conv_kernel_config->datatype_size;

      /* select the base register */
      if ( l_n > 26 ) {
        l_output_reg = i_gp_reg_mapping->gp_reg_help_6;
      } else if ( l_n > 17 ) {
        l_output_reg = i_gp_reg_mapping->gp_reg_help_5;
      } else if ( l_n > 8 ) {
        l_output_reg = i_gp_reg_mapping->gp_reg_help_4;
      } else {
        l_output_reg = i_gp_reg_mapping->gp_reg_output;
      }
      if ( l_n % 9 == 0 ) {
        l_output_idx = LIBXSMM_X86_GP_REG_UNDEF;
        l_scale = 0;
      } else if ( l_n % 9 == 1 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_0;
        l_scale = 1;
      } else if ( l_n % 9 == 2 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_0;
        l_scale = 2;
      } else if ( l_n % 9 == 3 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_1;
        l_scale = 1;
      } else if ( l_n % 9 == 4 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_0;
        l_scale = 4;
      } else if ( l_n % 9 == 5 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_2;
        l_scale = 1;
      } else if ( l_n % 9 == 6 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_1;
        l_scale = 2;
      } else if ( l_n % 9 == 7 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_3;
        l_scale = 1;
      } else if ( l_n % 9 == 8 ) {
        l_output_idx = i_gp_reg_mapping->gp_reg_help_0;
        l_scale = 8;
      } else {
        /* shouldn't happen.... */
      }

      libxsmm_x86_instruction_vec_compute_mem( io_generated_code,
                                               i_conv_kernel_config->instruction_set,
                                               i_conv_kernel_config->vfma_instruction,
                                               1,
                                               l_output_reg,
                                               l_output_idx,
                                               l_scale,
                                               l_disp,
                                               i_conv_kernel_config->vector_name,
                                               (l_k%4),
#ifdef OPT_LOAD_STORE
                                               i_conv_kernel_config->vector_reg_count - (i_conv_desc->ofw_rb* l_reg_per_block) + ((l_n+l_k_1)%(i_conv_desc->ofw_rb * l_reg_per_block)));
#else
                                               i_conv_kernel_config->vector_reg_count - (i_conv_desc->ofw_rb* l_reg_per_block) + l_n);
#endif

#ifdef ENABLE_OUTPUT_PREFETCH
      /* TODO POSSIBLE BUG: if kw is not unrolled and you run over the last iteration of l_k_1, you may access unwanted memory and the prefetches are junk... do not help */
      if ( (l_n == 3) && (l_k < (i_kh_unroll * i_conv_desc->ofw_padded)) && (i_kw_unroll > 1) && (l_k_1 == i_kw_unroll-1) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) ) {
        libxsmm_x86_instruction_prefetch( io_generated_code,
                                          LIBXSMM_X86_INSTR_PREFETCHT0 /*i_conv_kernel_config->prefetch_instruction*/,
                                          i_gp_reg_mapping->gp_reg_output_pf,
                                          LIBXSMM_X86_GP_REG_UNDEF,
                                          0,
                                          (l_k * i_conv_kernel_config->datatype_size * i_conv_desc->ofm_block) );
      }
#if 0
      if ( (l_n == 5) && (l_k_2 < (i_conv_desc->ofw)) && (l_k_3 < (i_kh_unroll -1)) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) ) {
        libxsmm_x86_instruction_prefetch( io_generated_code,
                                          LIBXSMM_X86_INSTR_PREFETCHT0 /*i_conv_kernel_config->prefetch_instruction*/,
                                          i_gp_reg_mapping->gp_reg_output,
                                          LIBXSMM_X86_GP_REG_UNDEF,
                                          0,
                                          i_conv_desc->ofw_padded * i_conv_desc->ofm_block * i_conv_kernel_config->datatype_size + (l_k_2 * i_conv_kernel_config->datatype_size * i_conv_desc->ofm_block) );
      }
#endif
#endif
#ifdef ENABLE_WEIGHT_PREFETCH
      /* prefetch to next ki to L2 */
      if ( (l_n == 8) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) ) {
        libxsmm_x86_instruction_prefetch( io_generated_code,
                                          i_conv_kernel_config->prefetch_instruction,
                                          /*LIBXSMM_X86_INSTR_PREFETCHT0,*/ /*i_conv_kernel_config->prefetch_instruction,*/
                                          i_gp_reg_mapping->gp_reg_weight_pf,
                                          LIBXSMM_X86_GP_REG_UNDEF, 0,
                                          /* TODO POSSIBLE BUG: if kw is not unrolled and you run over the last iteration of l_k_1, you may access unwanted memory and the prefetches are junk... do not help */
                                          (((((i_conv_desc->unroll_kw == 1) && (l_k_1 == i_kw_unroll-1)) ? 0 : (l_k_1 +  1))* i_conv_kernel_config->vector_length) - (l_k_3 * i_conv_desc->kw * i_conv_kernel_config->vector_length ) + (l_k_2))* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size);
      }
#endif

#ifdef OLD_PREFETCH_OUTPUT_L1
  /* prefetch for next kh loop and we are in the last iteration of l_k_2, bring for next kh loop */
  if(/*(l_n % 2 == 0) &&*/ (l_k_3 < i_kh_unroll-1) && (l_k_2 == (i_conv_desc->ofm_block -3)) && ((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1)) {
      libxsmm_x86_instruction_prefetch( io_generated_code,
                                     LIBXSMM_X86_INSTR_PREFETCHT0,
                                     i_gp_reg_mapping->gp_reg_output_pf,
                                     LIBXSMM_X86_GP_REG_UNDEF,
                                     0,
                                     (i_conv_desc->ofw_padded + l_n) * i_conv_desc->ofm_block * i_conv_kernel_config->datatype_size  );
  }
#endif

    } /* end of l_n over ofw_rb */
    l_displacement_k++;
  } /* end of l_k_2 */

  if(i_kh_unroll > 1) {
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_output,
                                     i_conv_desc->ofw_padded * i_conv_desc->ofm_block * i_conv_kernel_config->datatype_size  );
#if 0
#ifdef ENABLE_OUTPUT_PREFETCH
    if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1)) {
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_output_pf,
                                     i_conv_desc->ofw_padded * i_conv_desc->ofm_block * i_conv_kernel_config->datatype_size  );
    }
#endif
#endif
  if ( i_conv_desc->ofw_rb > 9 ) {
    libxsmm_x86_instruction_alu_reg( io_generated_code, i_conv_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_output, i_gp_reg_mapping->gp_reg_help_4);
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_help_4,  9 * i_conv_kernel_config->datatype_size * /*i_conv_desc->stride_w **/ i_conv_desc->ofm_block );
  }
  if ( i_conv_desc->ofw_rb > 18 ) {
    libxsmm_x86_instruction_alu_reg( io_generated_code, i_conv_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_output, i_gp_reg_mapping->gp_reg_help_5);
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_help_5, 18 *  i_conv_kernel_config->datatype_size * /*i_conv_desc->stride_w **/ i_conv_desc->ofm_block );
  }
  if ( i_conv_desc->ofw_rb > 27 ) {
    libxsmm_x86_instruction_alu_reg( io_generated_code, i_conv_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_output, i_gp_reg_mapping->gp_reg_help_6);
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_help_6, 27 *  i_conv_kernel_config->datatype_size * /*i_conv_desc->stride_w **/ i_conv_desc->ofm_block );
  }
#if 0
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_weight,  (i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ));
    if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L1)) {
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     (i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ));
    }
#endif
  }
  } /* Finish l_k_3 */

  if(i_kh_unroll > 1) {
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_output,
                                     i_kh_unroll * i_conv_desc->ofw_padded * i_conv_desc->ofm_block * i_conv_kernel_config->datatype_size  );
#if 0
#ifdef ENABLE_OUTPUT_PREFETCH
    if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_OUTPUT_L1)) {
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_output_pf,
                                     i_kh_unroll * i_conv_desc->ofw_padded * i_conv_desc->ofm_block * i_conv_kernel_config->datatype_size  );
    }
#endif
#endif
#if 0
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_weight,
                                     i_kh_unroll * (i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ));

    if(((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L1) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L1)) {
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     i_kh_unroll * (i_conv_desc->kw * i_conv_desc->ofm_block * (i_conv_kernel_config->vector_length) * i_conv_kernel_config->datatype_size  ));
    }
#endif
  }
#if 0
  /* we have to make sure that we are reseting the pointer to its original value in case a full unroll */
  if ( l_k_updates > 0 ) {
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_output, 8*l_k_updates* i_conv_desc->ofm_block * i_conv_kernel_config->datatype_size  );
  }
#endif

#ifdef OPT_LOAD_STORE
    reg_count = 0;
    if(l_k_1 == i_kw_unroll-1) {
    reg_count = 0;
        /* store inputs */
       for (j = 0; j < i_conv_desc->ofw_rb; j++ ) {
      for (k = 0; k < l_reg_per_block ; k++, reg_count++ ) {
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                    i_conv_kernel_config->instruction_set,
                                    i_conv_kernel_config->vmove_instruction,
                                    i_gp_reg_mapping->gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    (reg_count+l_k_1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size,
                                    i_conv_kernel_config->vector_name,
                                    l_vec_reg_acc_start + ((reg_count+l_k_1) % (i_conv_desc->ofw_rb * l_reg_per_block)) , 0, 1 /*store*/);
      }
    }

    } else {
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                    i_conv_kernel_config->instruction_set,
                                    i_conv_kernel_config->vmove_instruction,
                                    i_gp_reg_mapping->gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    (reg_count+l_k_1)*i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size,
                                    i_conv_kernel_config->vector_name,
                                    l_vec_reg_acc_start + ((reg_count+l_k_1) % (i_conv_desc->ofw_rb * l_reg_per_block)) , 0, 1 /*store*/);

    }
#else
    reg_count = 0;
        /* store inputs */
       for (j = 0; j < i_conv_desc->ofw_rb; j++ ) {
      for (k = 0; k < l_reg_per_block ; k++, reg_count++ ) {
            libxsmm_x86_instruction_vec_move( io_generated_code,
                                    i_conv_kernel_config->instruction_set,
                                    i_conv_kernel_config->vmove_instruction,
                                    i_gp_reg_mapping->gp_reg_input,
                                    LIBXSMM_X86_GP_REG_UNDEF, 0,
                                    (reg_count+l_k_1)* /*i_conv_desc->kw**/i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size,
                                    i_conv_kernel_config->vector_name,
                                    l_vec_reg_acc_start + reg_count , 0, 1 /*store*/);
    }
  }
#endif
    if(i_kw_unroll > 1) {
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_weight,
                                     i_conv_desc->ofm_block * i_conv_kernel_config->vector_length*i_conv_kernel_config->datatype_size  );
#if 0
#ifdef ENABLE_WEIGHT_PREFETCH
    if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) {
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     i_conv_desc->ofm_block * i_conv_kernel_config->vector_length*i_conv_kernel_config->datatype_size  );
    }
#endif
#endif
    }
  } /* end of l_k_1 */
  if(i_kw_unroll > 1) {
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_weight, i_kw_unroll * i_conv_desc->ofm_block * i_conv_kernel_config->vector_length*i_conv_kernel_config->datatype_size  );
#if 0
#ifdef ENABLE_WEIGHT_PREFETCH
    if((i_conv_desc->prefetch & LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) == LIBXSMM_CONVOLUTION_PREFETCH_WEIGHT_L2) {
      libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_weight_pf,
                                     i_kw_unroll * i_conv_desc->ofm_block * i_conv_kernel_config->vector_length*i_conv_kernel_config->datatype_size  );
    }
#endif
#endif
  }

#if 0
  /* we have to make sure that we are reseting the pointer to its original value in case a full unroll */
  if ( l_weight_updates > 0 ) {
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_conv_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_weight, l_weight_updates * 2 * i_conv_desc->ofm_block* i_conv_kernel_config->vector_length * i_conv_kernel_config->datatype_size  );
  }
#endif

}

