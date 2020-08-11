#include "../../SDP.hxx"
#include "set_dual_objective_b.hxx"

#include <boost/filesystem.hpp>

void read_blocks(const boost::filesystem::path &sdp_directory, SDP &sdp);
void read_objectives(const boost::filesystem::path &sdp_directory,
                     const El::Grid &grid, El::BigFloat &objective_const,
                     El::DistMatrix<El::BigFloat> &dual_objective_b);
void read_bilinear_bases(
  const boost::filesystem::path &sdp_directory, const Block_Info &block_info,
  const El::Grid &grid,
  std::vector<El::Matrix<El::BigFloat>> &bilinear_bases_local,
  std::vector<El::DistMatrix<El::BigFloat>> &bilinear_bases_dist);

void read_primal_objective_c(const boost::filesystem::path &sdp_directory,
                             const std::vector<size_t> &block_indices,
                             const El::Grid &grid,
                             Block_Vector &primal_objective_c);
void read_free_var_matrix(const boost::filesystem::path &sdp_directory,
                          const std::vector<size_t> &block_indices,
                          const El::Grid &grid, Block_Matrix &free_var_matrix);

SDP::SDP(const boost::filesystem::path &sdp_directory,
         const Block_Info &block_info, const El::Grid &grid)
{
  read_objectives(sdp_directory, grid, objective_const, dual_objective_b);
  read_bilinear_bases(sdp_directory, block_info, grid, bilinear_bases_local,
                      bilinear_bases_dist);
  read_primal_objective_c(sdp_directory, block_info.block_indices, grid,
                          primal_objective_c);
  read_free_var_matrix(sdp_directory, block_info.block_indices, grid,
                       free_var_matrix);
}

SDP::SDP(const std::vector<El::BigFloat> &objectives,
         const std::vector<El::BigFloat> &normalization,
         const Block_Info &block_info, const El::Grid &grid)
{
  // TODO: This is duplicated from sdp2input/write_output/write_output.cxx
  auto max_normalization(normalization.begin());
  for(auto n(normalization.begin()); n != normalization.end(); ++n)
    {
      if(Abs(*n) > Abs(*max_normalization))
        {
          max_normalization = n;
        }
    }
  size_t max_index(std::distance(normalization.begin(), max_normalization));

  objective_const = objectives.at(max_index) / normalization.at(max_index);

  std::vector<El::BigFloat> offset_dual_objective_b;
  offset_dual_objective_b.reserve(normalization.size() - 1);
  for(size_t index = 0; index < normalization.size(); ++index)
    {
      if(index != max_index)
        {
          offset_dual_objective_b.push_back(
            objectives.at(index) - normalization.at(index) * objective_const);
        }
    }

  set_dual_objective_b(offset_dual_objective_b, grid, dual_objective_b);
}