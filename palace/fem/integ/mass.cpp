// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "fem/integrator.hpp"

#include <vector>
#include <mfem.hpp>
#include "fem/libceed/coefficient.hpp"
#include "fem/libceed/integrator.hpp"

#include "fem/qfunctions/mass_qf.h"

namespace palace
{

struct MassIntegratorInfo : public ceed::IntegratorInfo
{
  MassContext ctx;
};

namespace
{

MassIntegratorInfo InitializeIntegratorInfo(const mfem::ParFiniteElementSpace &fespace,
                                            const mfem::IntegrationRule &ir,
                                            const std::vector<int> &indices, bool use_bdr,
                                            mfem::Coefficient *Q,
                                            mfem::VectorCoefficient *VQ,
                                            mfem::MatrixCoefficient *MQ,
                                            std::vector<ceed::QuadratureCoefficient> &coeff)
{
  MassIntegratorInfo info = {{0}};

  mfem::ParMesh &mesh = *fespace.GetParMesh();
  info.ctx.dim = mesh.Dimension() - use_bdr;
  info.ctx.space_dim = mesh.SpaceDimension();
  info.ctx.vdim = fespace.GetVDim();

  info.trial_op = ceed::EvalMode::Interp;
  info.test_op = ceed::EvalMode::Interp;

  mfem::ConstantCoefficient *const_coeff = dynamic_cast<mfem::ConstantCoefficient *>(Q);
  if (const_coeff || !(Q || VQ || MQ))
  {
    info.qdata_size = 1;
    info.ctx.coeff = const_coeff ? const_coeff->constant : 1.0;

    info.build_qf = f_build_mass_const_scalar;
    info.build_qf_path = PalaceQFunctionRelativePath(f_build_mass_const_scalar_loc);

    info.apply_qf = f_apply_mass_scalar;
    info.apply_qf_path = PalaceQFunctionRelativePath(f_apply_mass_scalar_loc);
  }
  else if (Q)
  {
    info.qdata_size = 1;
    ceed::InitCoefficient(*Q, mesh, ir, indices, use_bdr, coeff.emplace_back());

    info.build_qf = f_build_mass_quad_scalar;
    info.build_qf_path = PalaceQFunctionRelativePath(f_build_mass_quad_scalar_loc);

    info.apply_qf = f_apply_mass_scalar;
    info.apply_qf_path = PalaceQFunctionRelativePath(f_apply_mass_scalar_loc);
  }
  else if (VQ)
  {
    MFEM_VERIFY(VQ->GetVDim() == info.ctx.vdim,
                "Invalid vector coefficient dimension for vector MassIntegrator!");
    info.qdata_size = info.ctx.vdim;
    ceed::InitCoefficient(*VQ, mesh, ir, indices, use_bdr, coeff.emplace_back());

    info.build_qf = f_build_mass_quad_vector;
    info.build_qf_path = PalaceQFunctionRelativePath(f_build_mass_quad_vector_loc);

    info.apply_qf = f_apply_mass_vector;
    info.apply_qf_path = PalaceQFunctionRelativePath(f_apply_mass_vector_loc);
  }
  else if (MQ)
  {
    MFEM_VERIFY(MQ->GetVDim() == info.ctx.vdim,
                "Invalid matrix coefficient dimension for vector MassIntegrator!");
    info.qdata_size = (info.ctx.vdim * (info.ctx.vdim + 1)) / 2;
    ceed::InitCoefficient(*MQ, mesh, ir, indices, use_bdr, coeff.emplace_back());

    info.build_qf = f_build_mass_quad_matrix;
    info.build_qf_path = PalaceQFunctionRelativePath(f_build_mass_quad_matrix_loc);

    info.apply_qf = f_apply_mass_matrix;
    info.apply_qf_path = PalaceQFunctionRelativePath(f_apply_mass_matrix_loc);
  }

  return info;
}

}  // namespace

void MassIntegrator::Assemble(const mfem::ParFiniteElementSpace &trial_fespace,
                              const mfem::ParFiniteElementSpace &test_fespace,
                              const mfem::IntegrationRule &ir,
                              const std::vector<int> &indices, Ceed ceed, CeedOperator *op,
                              CeedOperator *op_t)
{
  MFEM_VERIFY(&trial_fespace == &test_fespace,
              "MassIntegrator requires the same test and trial spaces!");
  constexpr bool use_bdr = false;
  std::vector<ceed::QuadratureCoefficient> coeff;
  const auto info =
      InitializeIntegratorInfo(trial_fespace, ir, indices, use_bdr, Q, VQ, MQ, coeff);
  ceed::AssembleCeedOperator(info, trial_fespace, test_fespace, ir, indices, use_bdr, coeff,
                             ceed, op, op_t);
}

void MassIntegrator::AssembleBoundary(const mfem::ParFiniteElementSpace &trial_fespace,
                                      const mfem::ParFiniteElementSpace &test_fespace,
                                      const mfem::IntegrationRule &ir,
                                      const std::vector<int> &indices, Ceed ceed,
                                      CeedOperator *op, CeedOperator *op_t)
{
  MFEM_VERIFY(&trial_fespace == &test_fespace,
              "MassIntegrator requires the same test and trial spaces!");
  constexpr bool use_bdr = true;
  std::vector<ceed::QuadratureCoefficient> coeff;
  const auto info =
      InitializeIntegratorInfo(trial_fespace, ir, indices, use_bdr, Q, VQ, MQ, coeff);
  ceed::AssembleCeedOperator(info, trial_fespace, test_fespace, ir, indices, use_bdr, coeff,
                             ceed, op, op_t);
}

}  // namespace palace
