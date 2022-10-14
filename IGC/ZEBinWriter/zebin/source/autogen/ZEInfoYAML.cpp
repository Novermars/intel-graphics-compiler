/*========================== begin_copyright_notice ============================

Copyright (C) 2020-2022 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

//===- ZEInfoYAML.cpp -----------------------------------------------*- C++ -*-===//
// ZE Binary Utilitis
//
// file
//===----------------------------------------------------------------------===//

// ******************** DO NOT MODIFY DIRECTLY *********************************
// This file is auto-generated by ZEAutoTool/fileparser.py

#include <ZEInfoYAML.hpp>
using namespace zebin;
using namespace llvm::yaml;

void MappingTraits<zeInfoContainer>::mapping(IO& io, zeInfoContainer& info)
{
    io.mapRequired("version", info.version);
    io.mapRequired("kernels", info.kernels);
    io.mapOptional("functions", info.functions, FunctionsTy());
    io.mapOptional("global_host_access_table", info.global_host_access_table, HostAccessesTy());
    io.mapOptional("kernels_misc_info", info.kernels_misc_info, KernelsMiscInfoTy());
}
void MappingTraits<zeInfoKernel>::mapping(IO& io, zeInfoKernel& info)
{
    io.mapRequired("name", info.name);
    io.mapOptional("user_attributes", info.user_attributes, zeInfoUserAttribute());
    io.mapRequired("execution_env", info.execution_env);
    io.mapOptional("payload_arguments", info.payload_arguments, PayloadArgumentsTy());
    io.mapOptional("per_thread_payload_arguments", info.per_thread_payload_arguments, PerThreadPayloadArgumentsTy());
    io.mapOptional("binding_table_indices", info.binding_table_indices, BindingTableIndicesTy());
    io.mapOptional("per_thread_memory_buffers", info.per_thread_memory_buffers, PerThreadMemoryBuffersTy());
    io.mapOptional("inline_samplers", info.inline_samplers, InlineSamplersTy());
    io.mapOptional("experimental_properties", info.experimental_properties, zeInfoExperimentalProperties());
    io.mapOptional("debug_env", info.debug_env, zeInfoDebugEnv());
}
void MappingTraits<zeInfoFunction>::mapping(IO& io, zeInfoFunction& info)
{
    io.mapRequired("name", info.name);
    io.mapRequired("execution_env", info.execution_env);
}
void MappingTraits<zeInfoUserAttribute>::mapping(IO& io, zeInfoUserAttribute& info)
{
    io.mapOptional("intel_reqd_sub_group_size", info.intel_reqd_sub_group_size, 0);
    io.mapOptional("intel_reqd_workgroup_walk_order", info.intel_reqd_workgroup_walk_order);
    io.mapOptional("invalid_kernel", info.invalid_kernel, std::string());
    io.mapOptional("reqd_work_group_size", info.reqd_work_group_size);
    io.mapOptional("vec_type_hint", info.vec_type_hint, std::string());
    io.mapOptional("work_group_size_hint", info.work_group_size_hint);
}
void MappingTraits<zeInfoExecutionEnv>::mapping(IO& io, zeInfoExecutionEnv& info)
{
    io.mapOptional("barrier_count", info.barrier_count, 0);
    io.mapOptional("disable_mid_thread_preemption", info.disable_mid_thread_preemption, false);
    io.mapRequired("grf_count", info.grf_count);
    io.mapOptional("has_4gb_buffers", info.has_4gb_buffers, false);
    io.mapOptional("has_device_enqueue", info.has_device_enqueue, false);
    io.mapOptional("has_dpas", info.has_dpas, false);
    io.mapOptional("has_fence_for_image_access", info.has_fence_for_image_access, false);
    io.mapOptional("has_global_atomics", info.has_global_atomics, false);
    io.mapOptional("has_multi_scratch_spaces", info.has_multi_scratch_spaces, false);
    io.mapOptional("has_no_stateless_write", info.has_no_stateless_write, false);
    io.mapOptional("has_stack_calls", info.has_stack_calls, false);
    io.mapOptional("require_disable_eufusion", info.require_disable_eufusion, false);
    io.mapOptional("indirect_stateless_count", info.indirect_stateless_count, 0);
    io.mapOptional("inline_data_payload_size", info.inline_data_payload_size, 0);
    io.mapOptional("offset_to_skip_per_thread_data_load", info.offset_to_skip_per_thread_data_load, 0);
    io.mapOptional("offset_to_skip_set_ffid_gp", info.offset_to_skip_set_ffid_gp, 0);
    io.mapOptional("required_sub_group_size", info.required_sub_group_size, 0);
    io.mapOptional("required_work_group_size", info.required_work_group_size);
    io.mapRequired("simd_size", info.simd_size);
    io.mapOptional("slm_size", info.slm_size, 0);
    io.mapOptional("subgroup_independent_forward_progress", info.subgroup_independent_forward_progress, false);
    io.mapOptional("thread_scheduling_mode", info.thread_scheduling_mode, std::string());
    io.mapOptional("work_group_walk_order_dimensions", info.work_group_walk_order_dimensions);
}
void MappingTraits<zeInfoPayloadArgument>::mapping(IO& io, zeInfoPayloadArgument& info)
{
    io.mapRequired("arg_type", info.arg_type);
    io.mapRequired("offset", info.offset);
    io.mapRequired("size", info.size);
    io.mapOptional("arg_index", info.arg_index, -1);
    io.mapOptional("addrmode", info.addrmode, std::string());
    io.mapOptional("addrspace", info.addrspace, std::string());
    io.mapOptional("access_type", info.access_type, std::string());
    io.mapOptional("sampler_index", info.sampler_index, -1);
    io.mapOptional("source_offset", info.source_offset, -1);
    io.mapOptional("slm_alignment", info.slm_alignment, 0);
    io.mapOptional("image_type", info.image_type, std::string());
    io.mapOptional("image_transformable", info.image_transformable, false);
    io.mapOptional("sampler_type", info.sampler_type, std::string());
}
void MappingTraits<zeInfoPerThreadPayloadArgument>::mapping(IO& io, zeInfoPerThreadPayloadArgument& info)
{
    io.mapRequired("arg_type", info.arg_type);
    io.mapRequired("offset", info.offset);
    io.mapRequired("size", info.size);
}
void MappingTraits<zeInfoBindingTableIndex>::mapping(IO& io, zeInfoBindingTableIndex& info)
{
    io.mapRequired("bti_value", info.bti_value);
    io.mapRequired("arg_index", info.arg_index);
}
void MappingTraits<zeInfoPerThreadMemoryBuffer>::mapping(IO& io, zeInfoPerThreadMemoryBuffer& info)
{
    io.mapRequired("type", info.type);
    io.mapRequired("usage", info.usage);
    io.mapRequired("size", info.size);
    io.mapOptional("slot", info.slot, 0);
    io.mapOptional("is_simt_thread", info.is_simt_thread, false);
}
void MappingTraits<zeInfoInlineSampler>::mapping(IO& io, zeInfoInlineSampler& info)
{
    io.mapRequired("sampler_index", info.sampler_index);
    io.mapRequired("addrmode", info.addrmode);
    io.mapRequired("filtermode", info.filtermode);
    io.mapOptional("normalized", info.normalized, false);
}
void MappingTraits<zeInfoExperimentalProperties>::mapping(IO& io, zeInfoExperimentalProperties& info)
{
    io.mapOptional("has_non_kernel_arg_load", info.has_non_kernel_arg_load, -1);
    io.mapOptional("has_non_kernel_arg_store", info.has_non_kernel_arg_store, -1);
    io.mapOptional("has_non_kernel_arg_atomic", info.has_non_kernel_arg_atomic, -1);
}
void MappingTraits<zeInfoDebugEnv>::mapping(IO& io, zeInfoDebugEnv& info)
{
    io.mapOptional("sip_surface_bti", info.sip_surface_bti, -1);
    io.mapOptional("sip_surface_offset", info.sip_surface_offset, -1);
}
void MappingTraits<zeInfoHostAccess>::mapping(IO& io, zeInfoHostAccess& info)
{
    io.mapRequired("device_name", info.device_name);
    io.mapRequired("host_name", info.host_name);
}
void MappingTraits<zeInfoKernelMiscInfo>::mapping(IO& io, zeInfoKernelMiscInfo& info)
{
    io.mapRequired("name", info.name);
    io.mapOptional("args_info", info.args_info, ArgsInfoTy());
}
void MappingTraits<zeInfoArgInfo>::mapping(IO& io, zeInfoArgInfo& info)
{
    io.mapRequired("index", info.index);
    io.mapOptional("name", info.name, std::string());
    io.mapRequired("address_qualifier", info.address_qualifier);
    io.mapRequired("access_qualifier", info.access_qualifier);
    io.mapRequired("type_name", info.type_name);
    io.mapRequired("type_qualifiers", info.type_qualifiers);
}
