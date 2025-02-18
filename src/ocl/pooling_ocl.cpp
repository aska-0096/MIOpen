/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc.
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

#include <miopen/pooling.hpp>

#include <miopen/pooling/invoke_params.hpp>
#include <miopen/pooling/problem_description.hpp>
#include <miopen/pooling/solvers.hpp>
#include <miopen/check_numerics.hpp>
#include <miopen/datatype.hpp>
#include <miopen/float_equal.hpp>
#include <miopen/find_solution.hpp>
#include <miopen/kernel_cache.hpp>
#include <miopen/mlo_internal.hpp>

namespace miopen {

miopenStatus_t PoolingDescriptor::Forward(Handle& handle,
                                          const void* alpha,
                                          const TensorDescriptor& xDesc,
                                          ConstData_t x,
                                          const void* beta,
                                          const TensorDescriptor& yDesc,
                                          Data_t y,
                                          bool save_index,
                                          Data_t workSpace,
                                          size_t /*workSpaceSize*/) const
{

    if(!float_equal(*(static_cast<const float*>(alpha)), 1.0) ||
       !float_equal(*(static_cast<const float*>(beta)), 0))
    {
        MIOPEN_THROW("Only alpha=1 and beta=0 is supported");
    }
    if(miopen::CheckNumericsEnabled())
    {
        miopen::checkNumericsInput(handle, xDesc, x);
        if(!float_equal(*(static_cast<const float*>(beta)), 0))
        {
            miopen::checkNumericsInput(handle, yDesc, y);
        }
    }

    int pool_dim = xDesc.GetSize();
    if(pool_dim != 4 && pool_dim != 5)
    {
        MIOPEN_THROW("Unsupported pooling dimension");
    }

    auto index_max = get_index_max(GetIndexType());

    // for kernel implementation max pooling backward pass,
    //   "index_max" means ghost, and thus should not be reached
    if(mode == miopenPoolingMax && save_index)
    {
        if((workspaceIndexMode == miopenPoolingWorkspaceIndexMask &&
            !(index_max >= std::accumulate(lens.begin(), lens.end(), 1, std::multiplies<int>()))) ||
           (workspaceIndexMode == miopenPoolingWorkspaceIndexImage &&
            !(index_max >= std::accumulate(xDesc.GetLengths().begin() + 2,
                                           xDesc.GetLengths().end(),
                                           1,
                                           std::multiplies<int>()))))
        {
            MIOPEN_THROW("Index range not enough for max pooling bwd");
        }

        if(workspaceIndexMode == miopenPoolingWorkspaceIndexMask && pool_dim == 5)
        {
            MIOPEN_THROW("3D pooling doesn't support workspace index mask mode");
        }

        if(workSpace == nullptr)
        {
            throw std::invalid_argument("workSpace cannot be NULL in Forward Pooling MAX mode when "
                                        "backward pass is requested");
        }
    }

    const auto algo_name =
        AlgorithmName{pool_dim == 5 ? "miopenPoolingNdForward" : "miopenPooling2dForward"};
    const auto problem = pooling::ProblemDescription{*this, xDesc, yDesc, save_index};

    const auto invoke_params = [&]() {
        auto tmp      = pooling::FwdInvokeParams{};
        tmp.type      = InvokeType::Run;
        tmp.xDesc     = xDesc;
        tmp.yDesc     = yDesc;
        tmp.pooling   = *this;
        tmp.x         = x;
        tmp.y         = y;
        tmp.workspace = workSpace;
        return tmp;
    }();

    const auto solvers = solver::SolverContainer<solver::pooling::PoolingForward2d,
                                                 solver::pooling::PoolingForwardNd,
                                                 solver::pooling::TransposedPoolingFwd2d,
                                                 solver::pooling::TransposedPoolingFwdNd>{};

    solvers.ExecutePrimitive(handle, problem, algo_name, invoke_params);

    if(miopen::CheckNumericsEnabled())
    {
        miopen::checkNumericsOutput(handle, yDesc, y);
    }

    return miopenStatusSuccess;
}

miopenStatus_t PoolingDescriptor::Backward(Handle& handle,
                                           const void* alpha,
                                           const TensorDescriptor& yDesc,
                                           ConstData_t /*y*/,
                                           const TensorDescriptor& dyDesc,
                                           ConstData_t dy,
                                           const TensorDescriptor& xDesc,
                                           ConstData_t /*x*/,
                                           const void* beta,
                                           const TensorDescriptor& dxDesc,
                                           Data_t dx,
                                           ConstData_t workSpace) const
{

    if(!float_equal(*(static_cast<const float*>(alpha)), 1.0) ||
       !float_equal(*(static_cast<const float*>(beta)), 0))
    {
        MIOPEN_THROW("Only alpha=1 and beta=0 is supported");
    }
    if(miopen::CheckNumericsEnabled())
    {
        // miopen::checkNumericsInput(handle, yDesc, y); // not actually used?
        miopen::checkNumericsInput(handle, dyDesc, dy);
        // miopen::checkNumericsInput(handle, xDesc, x); // not actually used?
        if(!float_equal(*(static_cast<const float*>(beta)), 0))
        {
            miopen::checkNumericsInput(handle, dxDesc, dx);
        }
    }

    assert(yDesc.GetElementSize() == dyDesc.GetElementSize() &&
           xDesc.GetElementSize() == dxDesc.GetElementSize());

    int pool_dim = dyDesc.GetSize();
    if(pool_dim != 4 && pool_dim != 5)
    {
        MIOPEN_THROW("Unsupported pooling dimension");
    }

    miopenStatus_t status = miopenStatusSuccess;

    auto index_max = get_index_max(GetIndexType());

    // for kernel implementation max pooling backward pass,
    //   "index_max" means ghost, and thus should not be reached
    if(mode == miopenPoolingMax &&
       ((workspaceIndexMode == miopenPoolingWorkspaceIndexMask &&
         !(index_max >= std::accumulate(lens.begin(), lens.end(), 1, std::multiplies<int>()))) ||
        (workspaceIndexMode == miopenPoolingWorkspaceIndexImage &&
         !(index_max >= std::accumulate(xDesc.GetLengths().begin() + 2,
                                        xDesc.GetLengths().end(),
                                        1,
                                        std::multiplies<int>())))))
    {
        MIOPEN_THROW("Index range not enough for max pooling bwd");
    }

    if(mode == miopenPoolingMax && workspaceIndexMode == miopenPoolingWorkspaceIndexMask &&
       pool_dim == 5)
    {
        MIOPEN_THROW("3D pooling doesn't support workspace index mask mode");
    }

    if(mode == miopenPoolingMax && workSpace == nullptr)
    {
        throw std::invalid_argument("workSpace cannot be NULL in Backward Pooling MAX mode");
    }
    int pooling_method =
        (mode == miopenPoolingMax)
            ? MLO_POOLING_OP_MAX
            : ((mode == miopenPoolingAverage) ? MLO_POOLING_OP_AVE : MLO_POOLING_OP_AVE_INCLUSIVE);

    int batch = dyDesc.GetLengths()[0];
    int chal  = dyDesc.GetLengths()[1];

    int top_d = *(dyDesc.GetLengths().rbegin() + 2);
    if(pool_dim == 4)
        top_d = 1;
    int top_h = *(dyDesc.GetLengths().rbegin() + 1);
    int top_w = *(dyDesc.GetLengths().rbegin());

    int bot_d = *(dxDesc.GetLengths().rbegin() + 2);
    if(pool_dim == 4)
        bot_d = 1;
    int bot_h = *(dxDesc.GetLengths().rbegin() + 1);
    int bot_w = *(dxDesc.GetLengths().rbegin());

    int pix_w_per_work = 1;
    int pix_h_per_work = pool_dim == 4 ? 8 : 4;
    int pix_d_per_work = pool_dim == 4 ? 1 : 2;

    int pix_blk_w = std::max((bot_w + pix_w_per_work - 1) / pix_w_per_work, 1);
    int pix_blk_h = std::max((bot_h + pix_h_per_work - 1) / pix_h_per_work, 1);
    int pix_blk_d = std::max((bot_d + pix_d_per_work - 1) / pix_d_per_work, 1);

    int max_activ_workitem = 65536;
    int total_work         = batch * chal * pix_blk_w * pix_blk_h * pix_blk_d;
    int activ_work         = std::min(total_work, max_activ_workitem);

    size_t lcl_work = 64;
    size_t grp_num  = (activ_work + lcl_work - 1) / lcl_work;

    const auto problem        = pooling::ProblemDescription{*this, xDesc, yDesc, dxDesc, dyDesc};
    const auto network_config = problem.MakeNetworkConfig();
    const auto algo_name =
        AlgorithmName{pool_dim == 5 ? "miopenPoolingNdBackward" : "miopenPooling2dBackward"};

    auto&& kernels = handle.GetKernels(algo_name, network_config);
    if(!kernels.empty())
    {
        if(pool_dim == 4)
        {
            if(mode == miopenPoolingMax)
            {
                kernels.front()(dy,
                                dx,
                                workSpace,
                                static_cast<int>(pads[0]),
                                static_cast<int>(pads[1]),
                                static_cast<int>(chal),
                                static_cast<int>(dxDesc.GetLengths()[2]),
                                static_cast<int>(dxDesc.GetLengths()[3]),
                                static_cast<int>(dyDesc.GetLengths()[2]),
                                static_cast<int>(dyDesc.GetLengths()[3]),
                                static_cast<int>(dxDesc.GetStrides()[0]),
                                static_cast<int>(dxDesc.GetStrides()[1]),
                                static_cast<int>(dxDesc.GetStrides()[2]),
                                static_cast<int>(dyDesc.GetStrides()[0]),
                                static_cast<int>(dyDesc.GetStrides()[1]),
                                static_cast<int>(dyDesc.GetStrides()[2]));
            }
            else
            {
                kernels.front()(dy,
                                dx,
                                static_cast<int>(pads[0]),
                                static_cast<int>(pads[1]),
                                static_cast<int>(chal),
                                static_cast<int>(dxDesc.GetLengths()[2]),
                                static_cast<int>(dxDesc.GetLengths()[3]),
                                static_cast<int>(dyDesc.GetLengths()[2]),
                                static_cast<int>(dyDesc.GetLengths()[3]),
                                static_cast<int>(dxDesc.GetStrides()[0]),
                                static_cast<int>(dxDesc.GetStrides()[1]),
                                static_cast<int>(dxDesc.GetStrides()[2]),
                                static_cast<int>(dyDesc.GetStrides()[0]),
                                static_cast<int>(dyDesc.GetStrides()[1]),
                                static_cast<int>(dyDesc.GetStrides()[2]));
            }
        }
        else
        {
            if(mode == miopenPoolingMax)
            {
                kernels.front()(dy,
                                dx,
                                workSpace,
                                static_cast<uint>(pads[0]),
                                static_cast<uint>(pads[1]),
                                static_cast<uint>(pads[2]),
                                static_cast<uint>(batch),
                                static_cast<uint>(chal),
                                static_cast<uint>(dxDesc.GetLengths()[2]),
                                static_cast<uint>(dxDesc.GetLengths()[3]),
                                static_cast<uint>(dxDesc.GetLengths()[4]),
                                static_cast<uint>(top_d),
                                static_cast<uint>(top_h),
                                static_cast<uint>(top_w),
                                static_cast<uint>(dxDesc.GetStrides()[0]),
                                static_cast<uint>(dxDesc.GetStrides()[1]),
                                static_cast<uint>(dxDesc.GetStrides()[2]),
                                static_cast<uint>(dxDesc.GetStrides()[3]),
                                static_cast<uint>(dyDesc.GetStrides()[0]),
                                static_cast<uint>(dyDesc.GetStrides()[1]),
                                static_cast<uint>(dyDesc.GetStrides()[2]),
                                static_cast<uint>(dyDesc.GetStrides()[3]),
                                static_cast<uint>(total_work));
            }
            else
            {
                kernels.front()(dy,
                                dx,
                                static_cast<uint>(pads[0]),
                                static_cast<uint>(pads[1]),
                                static_cast<uint>(pads[2]),
                                static_cast<uint>(batch),
                                static_cast<uint>(chal),
                                static_cast<uint>(dxDesc.GetLengths()[2]),
                                static_cast<uint>(dxDesc.GetLengths()[3]),
                                static_cast<uint>(dxDesc.GetLengths()[4]),
                                static_cast<uint>(top_d),
                                static_cast<uint>(top_h),
                                static_cast<uint>(top_w),
                                static_cast<uint>(dxDesc.GetStrides()[0]),
                                static_cast<uint>(dxDesc.GetStrides()[1]),
                                static_cast<uint>(dxDesc.GetStrides()[2]),
                                static_cast<uint>(dxDesc.GetStrides()[3]),
                                static_cast<uint>(dyDesc.GetStrides()[0]),
                                static_cast<uint>(dyDesc.GetStrides()[1]),
                                static_cast<uint>(dyDesc.GetStrides()[2]),
                                static_cast<uint>(dyDesc.GetStrides()[3]),
                                static_cast<uint>(total_work));
            }
        }
    }
    else
    {
        if(pool_dim == 4)
        {
            mlo_construct_pooling2D construct_params(conv::Direction::BackwardData);
            construct_params.setStream(&handle);
            construct_params.setTopDfDescFromMLDesc(dyDesc);
            construct_params.setTopDescFromMLDesc(yDesc);
            construct_params.setBotDfDescFromMLDesc(dxDesc);
            construct_params.setBotDescFromMLDesc(xDesc);
            construct_params.setPoolingDescr(pooling_method,
                                             GetIndexType(),
                                             GetWorkspaceIndexMode(),
                                             lens[0],
                                             lens[1],
                                             pads[0],
                                             pads[1],
                                             strides[0],
                                             strides[1]);

            mloConstruct(construct_params);
            const std::vector<size_t>& vld = construct_params.getLocalWkSize();
            const std::vector<size_t>& vgd = construct_params.getGlobalWkSize();
            std::string program_name       = construct_params.getKernelFile(); // CL kernel
            std::string kernel_name        = construct_params.getKernelName(); // kernel name
            const std::string& parms = construct_params.getCompilerOptions();  // kernel parameters
            auto k                   = handle.AddKernel(
                algo_name, network_config, program_name, kernel_name, vld, vgd, parms);

            if(mode == miopenPoolingMax)
            {
                k(dy,
                  dx,
                  workSpace,
                  static_cast<int>(pads[0]),
                  static_cast<int>(pads[1]),
                  static_cast<int>(chal),
                  static_cast<int>(dxDesc.GetLengths()[2]),
                  static_cast<int>(dxDesc.GetLengths()[3]),
                  static_cast<int>(dyDesc.GetLengths()[2]),
                  static_cast<int>(dyDesc.GetLengths()[3]),
                  static_cast<int>(dxDesc.GetStrides()[0]),
                  static_cast<int>(dxDesc.GetStrides()[1]),
                  static_cast<int>(dxDesc.GetStrides()[2]),
                  static_cast<int>(dyDesc.GetStrides()[0]),
                  static_cast<int>(dyDesc.GetStrides()[1]),
                  static_cast<int>(dyDesc.GetStrides()[2]));
            }
            else
            {
                k(dy,
                  dx,
                  static_cast<int>(pads[0]),
                  static_cast<int>(pads[1]),
                  static_cast<int>(chal),
                  static_cast<int>(dxDesc.GetLengths()[2]),
                  static_cast<int>(dxDesc.GetLengths()[3]),
                  static_cast<int>(dyDesc.GetLengths()[2]),
                  static_cast<int>(dyDesc.GetLengths()[3]),
                  static_cast<int>(dxDesc.GetStrides()[0]),
                  static_cast<int>(dxDesc.GetStrides()[1]),
                  static_cast<int>(dxDesc.GetStrides()[2]),
                  static_cast<int>(dyDesc.GetStrides()[0]),
                  static_cast<int>(dyDesc.GetStrides()[1]),
                  static_cast<int>(dyDesc.GetStrides()[2]));
            }
        }
        else
        {
            std::string program_name = "MIOpenPoolingBwdND.cl";
            std::string kernel_name  = "mloPoolingND";
            if(mode == miopenPoolingMax)
            {
                kernel_name += "MaxBwd";
            }
            else if(mode == miopenPoolingAverage || mode == miopenPoolingAverageInclusive)
            {
                kernel_name += "AveBwd";
            }
            else
            {
                MIOPEN_THROW("Unknown backward pooling method");
            }

            const std::vector<size_t> vld{lcl_work, 1, 1};
            const std::vector<size_t> vgd{lcl_work * grp_num, 1, 1};

            std::string parms = std::string(" -DMLO_POOLING_OP_ID=") +
                                std::to_string(static_cast<long long>(pooling_method));

            parms += std::string(" -DMAX_ACTIV_WORKITEM=") +
                     std::to_string(static_cast<uint>(max_activ_workitem));

            parms += std::string(" -DMLO_POOLING_GROUP_SZ0=") +
                     std::to_string(static_cast<long long>(lcl_work)) +
                     std::string(" -DMLO_POOLING_GROUP_SZ1=1 -DMLO_POOLING_GROUP_SZ2=1");

            parms += std::string(" -DPIX_W_PER_WORK=") +
                     std::to_string(static_cast<uint>(pix_w_per_work)) +
                     std::string(" -DPIX_H_PER_WORK=") +
                     std::to_string(static_cast<uint>(pix_h_per_work)) +
                     std::string(" -DPIX_D_PER_WORK=") +
                     std::to_string(static_cast<uint>(pix_d_per_work));

            parms += std::string(" -DKERNEL_SZ_D=") + std::to_string(static_cast<uint>(lens[0])) +
                     std::string(" -DKERNEL_SZ_H=") + std::to_string(static_cast<uint>(lens[1])) +
                     std::string(" -DKERNEL_SZ_W=") + std::to_string(static_cast<uint>(lens[2])) +
                     std::string(" -DSTRIDE_D=") + std::to_string(static_cast<uint>(strides[0])) +
                     std::string(" -DSTRIDE_H=") + std::to_string(static_cast<uint>(strides[1])) +
                     std::string(" -DSTRIDE_W=") + std::to_string(static_cast<uint>(strides[2]));

            bool territory_overlap = false;
            for(std::size_t i = 0; i < strides.size(); i++)
            {
                territory_overlap |= (strides[i] < lens[i]);
            }

            parms += std::string(" -DTERRITORY_OVERLAP=") +
                     std::to_string(static_cast<int>(territory_overlap));

            parms += std::string(" -DMLO_POOLING_INDEX_TYPE=") +
                     get_pooling_index_type_name(GetIndexType()) +
                     std::string(" -DMLO_POOLING_INDEX_MAX=") +
                     get_pooling_index_type_max_name(GetIndexType()) +
                     GetDataTypeKernelParams(dyDesc.GetType());

            auto k = handle.AddKernel(
                algo_name, network_config, program_name, kernel_name, vld, vgd, parms);
            if(mode == miopenPoolingMax)
            {
                k(dy,
                  dx,
                  workSpace,
                  static_cast<uint>(pads[0]),
                  static_cast<uint>(pads[1]),
                  static_cast<uint>(pads[2]),
                  static_cast<uint>(batch),
                  static_cast<uint>(chal),
                  static_cast<uint>(dxDesc.GetLengths()[2]),
                  static_cast<uint>(dxDesc.GetLengths()[3]),
                  static_cast<uint>(dxDesc.GetLengths()[4]),
                  static_cast<uint>(top_d),
                  static_cast<uint>(top_h),
                  static_cast<uint>(top_w),
                  static_cast<uint>(dxDesc.GetStrides()[0]),
                  static_cast<uint>(dxDesc.GetStrides()[1]),
                  static_cast<uint>(dxDesc.GetStrides()[2]),
                  static_cast<uint>(dxDesc.GetStrides()[3]),
                  static_cast<uint>(dyDesc.GetStrides()[0]),
                  static_cast<uint>(dyDesc.GetStrides()[1]),
                  static_cast<uint>(dyDesc.GetStrides()[2]),
                  static_cast<uint>(dyDesc.GetStrides()[3]),
                  static_cast<uint>(total_work));
            }
            else
            {
                k(dy,
                  dx,
                  static_cast<uint>(pads[0]),
                  static_cast<uint>(pads[1]),
                  static_cast<uint>(pads[2]),
                  static_cast<uint>(batch),
                  static_cast<uint>(chal),
                  static_cast<uint>(dxDesc.GetLengths()[2]),
                  static_cast<uint>(dxDesc.GetLengths()[3]),
                  static_cast<uint>(dxDesc.GetLengths()[4]),
                  static_cast<uint>(top_d),
                  static_cast<uint>(top_h),
                  static_cast<uint>(top_w),
                  static_cast<uint>(dxDesc.GetStrides()[0]),
                  static_cast<uint>(dxDesc.GetStrides()[1]),
                  static_cast<uint>(dxDesc.GetStrides()[2]),
                  static_cast<uint>(dxDesc.GetStrides()[3]),
                  static_cast<uint>(dyDesc.GetStrides()[0]),
                  static_cast<uint>(dyDesc.GetStrides()[1]),
                  static_cast<uint>(dyDesc.GetStrides()[2]),
                  static_cast<uint>(dyDesc.GetStrides()[3]),
                  static_cast<uint>(total_work));
            }
        }
    }

    if(miopen::CheckNumericsEnabled())
    {
        miopen::checkNumericsOutput(handle, dxDesc, dx);
    }

    return (status);
}
} // namespace miopen
