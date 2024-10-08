#include "Output_SDP.hxx"

#include "pmp/max_normalization_index.hxx"

namespace
{
  // Translate polynomial vector matrix from (3.1) to (2.2)
  // TODO too many copies, pass by reference and modify in-place?
  [[nodiscard]] Polynomial_Vector_Matrix convert_pvm_using_normalization(
    const Polynomial_Vector_Matrix &input,
    const std::vector<El::BigFloat> &normalization,
    const size_t max_normalization_index)
  {
    Polynomial_Vector_Matrix output(input);

    for(int i = 0; i < output.polynomials.Height(); ++i)
      for(int j = 0; j < output.polynomials.Width(); ++j)
        {
          const auto &input_poly_vector = input.polynomials(i, j);
          auto &output_poly_vector = output.polynomials(i, j);

          const auto &poly_constant = output_poly_vector.at(0)
            = input_poly_vector.at(max_normalization_index)
              / normalization.at(max_normalization_index);

          size_t output_index = 1;
          for(size_t input_index = 0; input_index < normalization.size();
              ++input_index)
            {
              if(input_index == max_normalization_index)
                continue;

              const auto &input_coeffs
                = input_poly_vector.at(input_index).coefficients;
              auto &output_coeffs
                = output_poly_vector.at(output_index).coefficients;

              const auto size = std::max(input_coeffs.size(),
                                         poly_constant.coefficients.size());
              output_coeffs = input_coeffs;
              output_coeffs.resize(size);

              // p' = p - n*p_0
              // TODO implement opreator+() for Polynomial
              for(size_t degree = 0; degree < size; ++degree)
                {
                  if(degree < poly_constant.coefficients.size())
                    output_coeffs.at(degree)
                      -= normalization.at(input_index)
                         * poly_constant.coefficients.at(degree);
                }
              ++output_index;
            }
        }
    return output;
  }

  void validate(const Output_SDP &sdp)
  {
    ASSERT(sdp.num_blocks > 0);
    ASSERT(
      sdp.dual_constraint_groups.size() <= sdp.num_blocks,
      "sdp.dual_constraint_groups.size()=", sdp.dual_constraint_groups.size(),
      " should not exceed sdp.num_blocks=", sdp.num_blocks);
    for(const auto &group : sdp.dual_constraint_groups)
      {
        ASSERT(group.block_index < sdp.num_blocks,
               "group.block_index=", group.block_index,
               " should be less than sdp.num_blocks=", sdp.num_blocks);
      }
    // TODO: we should also check that block indices from all ranks
    // are unique and cover [0, num_blocks) range.
    // This is checked indirectly in write_sdp().
  }
}

Output_SDP::Output_SDP(const Polynomial_Matrix_Program &pmp,
                       const std::vector<std::string> &command_arguments,
                       Timers &timers)
    : normalization(pmp.normalization),
      num_blocks(pmp.num_matrices),
      command_arguments(command_arguments)
{
  Scoped_Timer timer(timers, "convert");

  // Trivial normalization: (1,0,0...0).
  const bool has_nontrivial_normalization = [this] {
    if(!normalization.has_value())
      return false;
    const El::BigFloat zero(0);
    for(size_t i = 0; i < normalization->size(); ++i)
      {
        if(i == 0 && normalization->at(i) != El::BigFloat(1))
          return true;
        if(i != 0 && normalization->at(i) != zero)
          return true;
      }
    return false;
  }();

  if(has_nontrivial_normalization)
    {
      const auto max_index = max_normalization_index(normalization.value());
      // (a_{max_index} / n_{max_index}) goes to b_0 = objective_const
      objective_const
        = pmp.objective.at(max_index) / normalization.value().at(max_index);
      // other a_i are translated to b_1..b_N = dual_objective_b
      dual_objective_b.reserve(pmp.objective.size() - 1);
      for(size_t index = 0; index < normalization.value().size(); ++index)
        {
          if(index != max_index)
            {
              dual_objective_b.push_back(pmp.objective.at(index)
                                         - normalization.value().at(index)
                                             * objective_const);
            }
        }

      Scoped_Timer pvm_timer(timers, "matrices");
      for(size_t i = 0; i < pmp.matrices.size(); ++i)
        {
          const auto &matrix = pmp.matrices.at(i);
          const auto block_index = pmp.matrix_index_local_to_global.at(i);
          dual_constraint_groups.emplace_back(
            block_index, convert_pvm_using_normalization(
                           matrix, normalization.value(), max_index));
        }
    }
  else
    {
      // b_0 = a_0
      objective_const = pmp.objective.at(0);
      // b_i = a_i
      dual_objective_b.reserve(pmp.objective.size() - 1);
      dual_objective_b.assign(pmp.objective.begin() + 1, pmp.objective.end());

      // If normalization == (1,0,0...0),
      // then all PVM matrices in (2.2) in SDPB Manual are the same as in (3.1),
      // and there is no need to convert them via convert_pvm_using_normalization()
      Scoped_Timer pvm_timer(timers, "matrices");
      for(size_t i = 0; i < pmp.matrices.size(); ++i)
        {
          const auto &matrix = pmp.matrices.at(i);
          const auto block_index = pmp.matrix_index_local_to_global.at(i);
          dual_constraint_groups.emplace_back(block_index, matrix);
        }
    }
  validate(*this);
}
