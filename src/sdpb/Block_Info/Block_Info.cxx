#include "../Block_Info.hxx"

#include "../../compute_block_grid_mapping.hxx"

#include <boost/filesystem/fstream.hpp>

namespace
{
  void
  read_vector_with_index(std::ifstream &input_stream,
                         const std::vector<size_t> &indices,
                         const size_t &index_scale, std::vector<size_t> &v)
  {
    std::vector<size_t> file_v;
    read_vector(input_stream, file_v);
    for(size_t index = 0; index < file_v.size(); ++index)
      {
        const size_t mapped_index(index_scale * indices[index / index_scale]
                                  + index % index_scale);
        if(mapped_index >= v.size())
          {
            v.resize(mapped_index + 1);
          }
        v[mapped_index] = file_v[index];
      }
  }
}

Block_Info::Block_Info(const boost::filesystem::path &sdp_directory,
                       const size_t &procs_per_node)
{
  size_t file_rank(0);
  do
    {
      boost::filesystem::ifstream block_stream(
        sdp_directory / ("blocks." + std::to_string(file_rank)));
      if(!block_stream.good())
        {
          throw std::runtime_error(
            "Could not open '"
            + (sdp_directory / ("blocks." + std::to_string(file_rank))).string()
            + "'");
        }
      block_stream >> file_num_procs;
      file_block_indices.emplace_back();
      auto &file_block_index(file_block_indices.back());
      read_vector(block_stream, file_block_index);

      read_vector_with_index(block_stream, file_block_index, 1, dimensions);
      read_vector_with_index(block_stream, file_block_index, 1, degrees);
      read_vector_with_index(block_stream, file_block_index, 1,
                             schur_block_sizes);
      read_vector_with_index(block_stream, file_block_index, 2,
                             psd_matrix_block_sizes);
      read_vector_with_index(block_stream, file_block_index, 2,
                             bilinear_pairing_block_sizes);
      ++file_rank;
    }
  while(file_rank < file_num_procs);

  boost::filesystem::ifstream objective_stream(sdp_directory / "objectives");
  double temp;
  size_t q;
  objective_stream >> temp >> q;
  if(!objective_stream.good())
    {
      throw std::runtime_error(
        "Could not read the size of the dual objective from '"
        + (sdp_directory / "objectives").string() + "'");
    }

  const size_t num_procs(El::mpi::Size(El::mpi::COMM_WORLD));
  const boost::filesystem::path block_costs_file(sdp_directory
                                                 / "block_timings");
  std::vector<Block_Cost> block_costs;
  if(boost::filesystem::exists(block_costs_file))
    {
      size_t index(0), cost;
      boost::filesystem::ifstream costs(block_costs_file);
      costs >> cost;
      while(costs.good())
        {
          block_costs.emplace_back(cost, index);
          ++index;
          costs >> cost;
        }
      if(block_costs.size() != schur_block_sizes.size())
        {
          throw std::runtime_error(
            "Incompatible number of entries in '"
            + (sdp_directory / "costs").string() + "'. Expected "
            + std::to_string(schur_block_sizes.size()) + " but found "
            + std::to_string(block_costs.size()));
        }
    }
  else
    {
      // This simulates round-robin by making everything cost the same
      // but with a round-robin order.
      for(size_t rank = 0; rank < num_procs; ++rank)
        {
          for(size_t block = rank; block < schur_block_sizes.size();
              block += num_procs)
            {
              block_costs.emplace_back(1, block);
            }
        }
    }

  // Reverse sort, with largest first
  std::sort(block_costs.rbegin(), block_costs.rend());
  if(num_procs % procs_per_node != 0)
    {
      throw std::runtime_error(
        "Incompatible number of processes and processes per node.  "
        "procs_per_node must evenly divide num_procs:\n\tnum_procs: "
        + std::to_string(num_procs)
        + "\n\tprocs_per_node: " + std::to_string(procs_per_node));
    }
  const size_t num_nodes(num_procs / procs_per_node);
  std::vector<std::vector<Block_Map>> mapping(
    compute_block_grid_mapping(procs_per_node, num_nodes, block_costs));

  // Create an mpi::Group for each set of processors.
  El::mpi::Group default_mpi_group;
  El::mpi::CommGroup(El::mpi::COMM_WORLD, default_mpi_group);

  int rank(El::mpi::Rank(El::mpi::COMM_WORLD));

  if(rank == 0)
    {
      std::stringstream ss;
      ss << "Block Grid Mapping\n"
         << "Node\tNum Procs\tCost\t\tBlock List\n"
         << "==================================================\n";
      for(size_t node = 0; node < mapping.size(); ++node)
        {
          for(auto &m : mapping[node])
            {
              ss << node << "\t" << m.num_procs << "\t\t"
                 << m.cost / static_cast<double>(m.num_procs) << "\t{";
              for(size_t ii = 0; ii < m.block_indices.size(); ++ii)
                {
                  if(ii != 0)
                    {
                      ss << ",";
                    }
                  ss << "(" << m.block_indices[ii] << ","
                     << schur_block_sizes[m.block_indices[ii]] << ")";
                }
              ss << "}\n";
            }
          ss << "\n";
        }
      El::Output(ss.str());
    }

  int rank_begin(0), rank_end(0);
  for(auto &block_vector : mapping)
    {
      for(auto &block_map : block_vector)
        {
          rank_begin = rank_end;
          rank_end += block_map.num_procs;
          if(rank_end > rank)
            {
              block_indices = block_map.block_indices;
              break;
            }
        }
      if(rank_end > rank)
        {
          break;
        }
    }
  // We should be generating blocks to cover all of the processors,
  // even if there are more nodes than procs.  So this is a sanity
  // check in case we messed up something in
  // compute_block_grid_mapping.
  if(rank_end <= rank)
    {
      throw std::runtime_error("INTERNAL ERROR: Some procs were not covered "
                               "by compute_block_grid_mapping.\n"
                               "\trank = "
                               + std::to_string(rank) + "\n"
                               + "\trank_end = " + std::to_string(rank_end));
    }
  {
    std::vector<int> group_ranks(rank_end - rank_begin);
    std::iota(group_ranks.begin(), group_ranks.end(), rank_begin);
    El::mpi::Incl(default_mpi_group, group_ranks.size(), group_ranks.data(),
                  mpi_group);
  }
  El::mpi::Create(El::mpi::COMM_WORLD, mpi_group, mpi_comm);
}
