#include "../Function_Blocks_Parser.hxx"

bool Function_Blocks_Parser::EndObject(rapidjson::SizeType)
{
  if(inside)
    {
      if(parsing_objective)
        {
          throw std::runtime_error(
            "Invalid input file.  Found an object end inside '"
            + objective_state.name + "'");
        }
      else if(parsing_normalization)
        {
          throw std::runtime_error(
            "Invalid input file.  Found an object end inside '"
            + normalization_state.name + "'");
        }
      else if(parsing_points)
        {
          throw std::runtime_error(
            "Invalid input file.  Found an object end inside '"
            + points_state.name + "'");
        }
      else if(parsing_functions)
        {
          throw std::runtime_error(
            "Invalid input file.  Found an object end inside '"
            + functions_state.name + "'");
        }
      else
        {
          inside = false;
        }
    }
  else
    {
      throw std::runtime_error(
        "Invalid input file.  Ending an object outside of the SDP");
    }
  return true;
}
