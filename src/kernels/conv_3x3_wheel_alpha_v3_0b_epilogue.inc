/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2019 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/
s_endpgm
.Lfunc_end0:
   .size miopenSp3AsmConv3x3F, .Lfunc_end0 - miopenSp3AsmConv3x3F

.if ROCM_METADATA_VERSION == 5
workgroup_size_x=512
.amdgcn.next_free_sgpr = 92
.amdgcn.next_free_vgpr = 128

//xnack disabled by default for asm kernels
__sgpr_reserve_vcc_default = 1
__sgpr_reserve_xnack_default = 0
__sgpr_reserve_flatscr_default = 0

.rodata
.p2align 6
.amdhsa_kernel miopenSp3AsmConv3x3F
    .amdhsa_group_segment_fixed_size 32768
    .amdhsa_user_sgpr_private_segment_buffer 1
    .amdhsa_user_sgpr_dispatch_ptr 1
    .amdhsa_user_sgpr_kernarg_segment_ptr 1
    .amdhsa_system_sgpr_workgroup_id_x 1
    .amdhsa_system_sgpr_workgroup_id_y 0
    .amdhsa_system_sgpr_workgroup_id_z 0
    .amdhsa_system_vgpr_workitem_id 0
    .amdhsa_next_free_vgpr .amdgcn.next_free_vgpr
    .amdhsa_next_free_sgpr .amdgcn.next_free_sgpr
    .amdhsa_reserve_vcc __sgpr_reserve_vcc_default
    .amdhsa_reserve_xnack_mask __sgpr_reserve_xnack_default
    .amdhsa_reserve_flat_scratch __sgpr_reserve_flatscr_default
    .amdhsa_ieee_mode 0
    .amdhsa_dx10_clamp 0
.end_amdhsa_kernel

.macro METADATA sc,wc,wg_x
.amdgpu_metadata
---
amdhsa.version: [ 1, 0 ]
amdhsa.kernels:
  - .name: miopenSp3AsmConv3x3F
    .symbol: miopenSp3AsmConv3x3F.kd
    .language: "OpenCL C"
    .language_version: [ 1, 2 ]
    .sgpr_count: \sc
    .vgpr_count: \wc
    .group_segment_fixed_size: 32768
    .private_segment_fixed_size: 0
    .kernarg_segment_size: 112
    .kernarg_segment_align: 8
    .reqd_workgroup_size: [ \wg_x, 1, 1 ]
    .max_flat_workgroup_size: \wg_x
    .wavefront_size: 64
    .args:
    - { .size: 4, .offset: 0,   .value_kind: by_value,      .value_type: i32, .name: N }
    - { .size: 4, .offset: 4,   .value_kind: by_value,      .value_type: i32, .name: C }
    - { .size: 4, .offset: 8,   .value_kind: by_value,      .value_type: i32, .name: H }
    - { .size: 4, .offset: 12,  .value_kind: by_value,      .value_type: i32, .name: W }
    - { .size: 4, .offset: 16,  .value_kind: by_value,      .value_type: i32, .name: K }
    - { .size: 4, .offset: 20,  .value_kind: by_value,      .value_type: i32, .name: n_groups }
    - { .size: 4, .offset: 24,  .value_kind: by_value,      .value_type: i32, .name: flags }
    - { .size: 4, .offset: 28,  .value_kind: by_value,      .value_type: i32, .name: reserved }
    - { .size: 8, .offset: 32,  .value_kind: global_buffer, .value_type: f32, .name: in,      .address_space: global, .is_const: false }
    - { .size: 8, .offset: 40,  .value_kind: global_buffer, .value_type: f32, .name: weights, .address_space: global, .is_const: false }
    - { .size: 8, .offset: 48,  .value_kind: global_buffer, .value_type: f32, .name: out,     .address_space: global, .is_const: false }
    - { .size: 8, .offset: 56,  .value_kind: global_buffer, .value_type: i32, .name: rsv_ptr, .address_space: global, .is_const: false }
    - { .size: 8, .offset: 64,  .value_kind: hidden_global_offset_x, .value_type: i64 }
    - { .size: 8, .offset: 72,  .value_kind: hidden_global_offset_y, .value_type: i64 }
    - { .size: 8, .offset: 80,  .value_kind: hidden_global_offset_z, .value_type: i64 }
    - { .size: 8, .offset: 88,  .value_kind: hidden_none,   .value_type: i8 }
    - { .size: 8, .offset: 96,  .value_kind: hidden_none,   .value_type: i8 }
    - { .size: 8, .offset: 104, .value_kind: hidden_none,   .value_type: i8 }
...
.end_amdgpu_metadata
.endm // METADATA

.altmacro
.macro METADATA_WRAPPER sc,wc,wg_x
    METADATA %\sc, %\wc, %\wg_x
.endm

.ifnotdef workgroup_size_x
    .error "workgroup_size_x must be defined"
    .end
.endif

total_sgpr_count = .amdgcn.next_free_sgpr + 4 // vcc, xnack

METADATA_WRAPPER total_sgpr_count,.amdgcn.next_free_vgpr,workgroup_size_x

.elseif ROCM_METADATA_VERSION == 4
.amd_amdgpu_hsa_metadata
Version:         [ 1, 0 ]
Kernels:
  - Name:            miopenSp3AsmConv3x3F
    SymbolName:      'miopenSp3AsmConv3x3F@kd'
    Language:        OpenCL C
    LanguageVersion: [ 1, 2 ]
    Attrs:
      ReqdWorkGroupSize: [ 512, 1, 1 ]
    Args:
      - TypeName:        int
        Size:            4
        Align:           4
        ValueKind:       ByValue
        ValueType:       I32
        AccQual:         Default
      - TypeName:        int
        Size:            4
        Align:           4
        ValueKind:       ByValue
        ValueType:       I32
        AccQual:         Default
      - TypeName:        int
        Size:            4
        Align:           4
        ValueKind:       ByValue
        ValueType:       I32
        AccQual:         Default
      - TypeName:        int
        Size:            4
        Align:           4
        ValueKind:       ByValue
        ValueType:       I32
        AccQual:         Default
      - TypeName:        int
        Size:            4
        Align:           4
        ValueKind:       ByValue
        ValueType:       I32
        AccQual:         Default
      - TypeName:        int
        Size:            4
        Align:           4
        ValueKind:       ByValue
        ValueType:       I32
        AccQual:         Default
      - TypeName:        int
        Size:            4
        Align:           4
        ValueKind:       ByValue
        ValueType:       I32
        AccQual:         Default
      - TypeName:        int
        Size:            4
        Align:           4
        ValueKind:       ByValue
        ValueType:       I32
        AccQual:         Default
      - TypeName:        'float*'
        Size:            8
        Align:           8
        ValueKind:       GlobalBuffer
        ValueType:       F32
        AddrSpaceQual:   Global
        AccQual:         Default
      - TypeName:        'float*'
        Size:            8
        Align:           8
        ValueKind:       GlobalBuffer
        ValueType:       F32
        AddrSpaceQual:   Global
        AccQual:         Default
      - TypeName:        'float*'
        Size:            8
        Align:           8
        ValueKind:       GlobalBuffer
        ValueType:       F32
        AddrSpaceQual:   Global
        AccQual:         Default
      - TypeName:        'int*'
        Size:            8
        Align:           8
        ValueKind:       GlobalBuffer
        ValueType:       I32
        AddrSpaceQual:   Global
        AccQual:         Default
      - Size:            8
        Align:           8
        ValueKind:       HiddenGlobalOffsetX
        ValueType:       I64
      - Size:            8
        Align:           8
        ValueKind:       HiddenGlobalOffsetY
        ValueType:       I64
      - Size:            8
        Align:           8
        ValueKind:       HiddenGlobalOffsetZ
        ValueType:       I64
      - Size:            8
        Align:           8
        ValueKind:       HiddenNone
        ValueType:       I8
        AddrSpaceQual:   Global
      - Size:            8
        Align:           8
        ValueKind:       HiddenNone
        ValueType:       I8
        AddrSpaceQual:   Global
      - Size:            8
        Align:           8
        ValueKind:       HiddenNone
        ValueType:       I8
        AddrSpaceQual:   Global
    CodeProps:
      KernargSegmentSize: 112
      GroupSegmentFixedSize: 32768
      PrivateSegmentFixedSize: 0
      KernargSegmentAlign: 8
      WavefrontSize:   64
      NumVGPRs:        128
      MaxFlatWorkGroupSize: 512
...
.end_amd_amdgpu_hsa_metadata

.else
.error "Unsupported ROCM_METADATA_VERSION"
.end
.endif
