/* Copyright (C) 2011 Xavier Pujol
   (C) 2014-2016 Martin R. Albrecht
   (C) 2016 Michael Walter

   This file is part of fplll. fplll is free software: you
   can redistribute it and/or modify it under the terms of the GNU Lesser
   General Public License as published by the Free Software Foundation,
   either version 2.1 of the License, or (at your option) any later version.

   fplll is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with fplll. If not, see <http://www.gnu.org/licenses/>. */

#ifndef FPLLL_BKZ_H
#define FPLLL_BKZ_H

#include "bkz_param.h"
#include "enum/enumerate.h"
#include "enum/evaluator.h"
#include "lll.h"

FPLLL_BEGIN_NAMESPACE

/**
 * @brief Performs a heuristic check if BKZ can be terminated.
 *
 * Checks if the slope of the basis hasn't decreased in a while.
*/
template <class ZT, class FT> class BKZAutoAbort
{
public:

  /**
   * @brief Create an BKZAutoAbort object.  
   * 
   * @param m
   *    GSO object of the basis to be tested
   * @param num_rows
   *    the number of vectors to check
   * @param start_row
   *    the starting point of the vectors to check
  */
  BKZAutoAbort(MatGSO<ZT, FT> &m, int num_rows, int start_row = 0)
      : m(m), old_slope(numeric_limits<double>::max()), no_dec(-1), num_rows(num_rows),
        start_row(start_row)
  {
  }

  /**
   * @brief Performs the check
   *
   * Performs the actual check by computing the new slope and checks if
   * it has decreased. Keeps track of the number of times it has not
   * decreased and returns true if that number is larger than maxNoDec.
   *
   * @param scale
   *    slack parameter on the slope (i.e. slope has to decrease by at
   *    at least a multiplicative factor of scale)
   * @param maxNoDec
   *    the number of successive non-decreases in the slope before true
   *    is returned
   *
   * @returns
   *    true, if the slope has not decreased for maxNoDec calls
   *    false otherwise
   */
  bool test_abort(double scale = 1.0, int maxNoDec = 5);

private:
  MatGSO<ZT, FT> &m;
  double old_slope;
  int no_dec;
  int num_rows;
  int start_row;
};

/**
   * @brief The class performing block reduction.
   *
   * This class implements BKZ, SD-BKZ and Slide Reduction. For this
   * it relies on the GSO, LLL, and Enumeration modules. It assumes
   * that the basis is LLL reduced.
**/
template <class ZT, class FT> class BKZReduction
{
  /**
   * @brief Create a BKZObject
   * 
   * @param m
   *    GSO object corresponding to the basis to be reduced
   * @param lll_obj
   *    LLL object associated to the same GSO object m
   * @param param
   *    parameter object (see bkz_param.h)
   */
public:
  BKZReduction(MatGSO<ZT, FT> &m, LLLReduction<ZT, FT> &lll_obj, const BKZParam &param);
  ~BKZReduction();

  /**
   * @brief Preprocesses a block
   *
   * Preprocess a block using LLL or stronger recursive preprocessing.
   *
   * @param kappa
   *    start of the block
   * @param block_size
   *    size of the block
   * @param param
   *    parameter object for the current block size (the parameter object for the recursive 
   *    calls will be created in this function using the information from this object)
   *@returns
   *    false if it modified the basis, true otherwise
   */
  bool svp_preprocessing(int kappa, int block_size, const BKZParam &param);

  /**
   * @brief Inserts given (dual) vector into the basis
   *
   * Inserts a (dual) vector into the basis. This function does not
   * produce any linear dependencies, i.e. the result is a basis with the
   * specified (dual) vector in the first (resp, last) position, but there are no
   * guarantees beyond that, i.e. the basis might not be LLL reduced or even
   * size reduced.
   *
   * @param kappa
   *    start of the block
   * @param block_size
   *    size of the block
   * @param solution
   *    the coefficients of the (dual) vector in the current (dual) basis
   * @param dual
   *    flag specifying if 'solution' is a dual or primal vector and to be
   *    inserted into the basis or its dual
   * @returns
   *    false if it made progress, true otherwise
   */
  bool svp_postprocessing(int kappa, int block_size, const vector<FT> &solution, bool dual = false);

  /**
   * @brief (d)SVP-reduce a block.
   *
   * Ensures that the first (resp. last) vector in a block of the (dual) basis
   * is the shortest vector in the projected lattice generated by the block
   * (or its dual, resp.). This is implemented using pruned enumeration
   * with rerandomization. The results returned by the enumeration are inserted using
   * postprocessing, and so are no guarantees beyond that, i.e. the basis
   * might not be LLL reduced or even size reduced.
   *
   * @param kappa
   *    start of the block
   * @param block_size
   *    size of the block
   * @param param
   *    parameter object (may be different from the one in the constructor)
   * @param dual
   *    flag specifying if the block is to be SVP or dual SVP reduced.
   * @returns
   *    false if it made progress, true otherwise
  */
  bool svp_reduction(int kappa, int block_size, const BKZParam &param, bool dual = false);

  /**
   * @brief Same as svp_reduction(), but catches exceptions and returns with
   * error status.
   *
   * Essentially does the same as svp_reduction() with two differences: 
   *    1) it has as additional parameter a clean flag, which is set instead of returned.
   *    2) it catches failures in the reductions and instead of throwing an exception and
   *        returns false in case of a failure (true in case of success).
   *
   * @param kappa
   *    start of the block
   * @param block_size
   *    size of the block
   * @param param
   *    parameter object (may be different from the one in the constructor)
   * @param clean
   *    clean flag which is set to false if the first (last dual) vector is reduced.
   *    Otherwise the flag is left unchanged.
   * @param dual
   *    flag specifying if the block is to be SVP or dual SVP reduced.
   * @returns
   *    true if the reduction did no throw an error, otherwise false.
   */
  bool svp_reduction_ex(int kappa, int block_size, const BKZParam &param, bool &clean,
                        bool dual = false)
  {
    try
    {
      clean = svp_reduction(kappa, block_size, param, dual);
      return true;
    }
    catch (RedStatus &e)
    {
      return set_status(e);
    }
  }

  /**
   * @brief Runs a BKZ tour.
   *
   * Runs a BKZ tour from min_row to max_row by successively calling svp_reduction.
   *
   * @param loop
   *    counter indicating the iteration, only for reporting purposes
   * @param kappa_max
   *    the largest kappa s.t. the block from min_row to kappa is BKZ reduced, also
   *    only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the tour
   * @param max_row
   *    end of the tour
   * @return
   *    false if it made progress, true otherwise
   */
  bool tour(const int loop, int &kappa_max, const BKZParam &param, int min_row, int max_row);

  /**
   * @brief Same as tour(), but catches exceptions and returns with
   * error status.
   *
   * Essentially does the same as tour() with two differences: 
   *    1) it has as additional parameter a clean flag, which is set instead of returned.
   *    2) it catches failures in the reductions and instead of throwing an exception and
   *        returns false in case of a failure (true in case of success).
   * @param loop
   *    counter indicating the iteration, only for reporting purposes
   * @param kappa_max
   *    the largest kappa s.t. the block from min_row to kappa is BKZ reduced, also
   *    only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the tour
   * @param max_row
   *    end of the tour
   * @param clean
   *    clean flag which is set to false if the first (last dual) vector is reduced.
   *    Otherwise the flag is left unchanged.
   * @returns
   *    true if the tour did no throw an error, otherwise false.
   */
  bool tour_ex(const int loop, int &kappa_max, const BKZParam &param, int min_row, int max_row,
               bool &clean)
  {
    try
    {
      clean = tour(loop, kappa_max, param, min_row, max_row);
      return true;
    }
    catch (RedStatus &e)
    {
      return set_status(e);
    }
  }

  /**
   * @brief Runs a SD-BKZ tour.
   *
   * Runs a dual BKZ tour from max_row to min_row and a BKZ tour from min_row to max_row
   * by successively calling svp_reduction.
   *
   * @param loop
   *    counter indicating the iteration, only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the tour
   * @param max_row
   *    end of the tour
   * @return
   *    false if it made progress, true otherwise
   */
  bool sd_tour(const int loop, const BKZParam &param, int min_row, int max_row);

  /**
   * @brief Same as sd_tour(), but catches exceptions and returns with
   * error status.
   *
   * Essentially does the same as sd_tour() with two differences: 
   *    1) it has as additional parameter a clean flag, which is set instead of returned.
   *    2) it catches failures in the reductions and instead of throwing an exception and
   *        returns false in case of a failure (true in case of success).
   *  
   * @param loop
   *    counter indicating the iteration, only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the tour
   * @param max_row
   *    end of the tour
   * @param clean
   *    clean flag which is set to false if the first (last dual) vector is reduced.
   *    Otherwise the flag is left unchanged.
   * @returns
   *    true if the tour did no throw an error, otherwise false.
   */
  bool sd_tour_ex(const int loop, const BKZParam &param, int min_row, int max_row, bool &clean)
  {
    try
    {
      clean = sd_tour(loop, param, min_row, max_row);
      return true;
    }
    catch (RedStatus &e)
    {
      return set_status(e);
    }
  }

  /**
   * @brief HKZ reduces a block.
   *
   * Runs HKZ reduction from min_row to max_row by successively calling svp_reduction.
   *
   * @param kappa_max
   *    the largest kappa s.t. the block from min_row to kappa is BKZ reduced, also
   *    only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the tour
   * @param max_row
   *    end of the tour
   * @return
   *    false if it made progress, true otherwise
   */
  bool hkz(int &kappa_max, const BKZParam &param, int min_row, int max_row);

  /**
   * @brief Same as hkz(), but catches exceptions and returns with
   * error status.
   *
   * Essentially does the same as hkz() with two differences: 
   *    1) it has as additional parameter a clean flag, which is set instead of returned.
   *    2) it catches failures in the reductions and instead of throwing an exception and
   *        returns false in case of a failure (true in case of success).
   * 
   * @param kappa_max
   *    the largest kappa s.t. the block from min_row to kappa is BKZ reduced, also
   *    only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the reduction
   * @param max_row
   *    end of the reduction
   * @param clean
   *    clean flag which is set to false if the first (last dual) vector is reduced.
   *    Otherwise the flag is left unchanged.
   * @returns
   *    true if the reduction did no throw an error, otherwise false.
   */
  bool hkz_ex(int &kappa_max, const BKZParam &param, int min_row, int max_row, bool &clean)
  {
    try
    {
      clean = hkz(kappa_max, param, min_row, max_row);
      return true;
    }
    catch (RedStatus &e)
    {
      return set_status(e);
    }
  }

  /**
   * @brief Runs a tour of slide reduction.
   *
   * Runs a tour of slide reduction from min_row to max_row by
   *  1) alternating LLL and svp reductions on disjoint blocks
   *  2) dual svp reductions on slightly shifted disjoint blocks
   *
   * @param loop
   *    counter indicating the iteration, only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the tour
   * @param max_row
   *    end of the tour
   * @return
   *    false if it made progress, true otherwise
   */
  bool slide_tour(const int loop, const BKZParam &param, int min_row, int max_row);

  /**
   * @brief Same as slide_tour(), but catches exceptions and returns with
   * error status.
   *
   * Essentially does the same as slide_tour()with two differences: 
   *    1) it has as additional parameter a clean flag, which is set instead of returned.
   *    2) it catches failures in the reductions and instead of throwing an exception and
   *        returns false in case of a failure (true in case of success).
   * 
   * @param loop
   *    counter indicating the iteration, only for reporting purposes
   * @param param
   *    parameter object
   * @param min_row
   *    start of the tour
   * @param max_row
   *    end of the tour
   * @param clean
   *    clean flag which is set to false if the first (last dual) vector is reduced.
   *    Otherwise the flag is left unchanged.
   * @returns
   *    true if the tour did no throw an error, otherwise false.
   */
  bool slide_tour_ex(const int loop, const BKZParam &param, int min_row, int max_row, bool &clean)
  {
    try
    {
      clean = slide_tour(loop, param, min_row, max_row);
      return true;
    }
    catch (RedStatus &e)
    {
      return set_status(e);
    }
  }

  /**
   * @brief Runs the main loop of block reduction.
   *
   * Top level function implementing block reduction by repeatedly
   * calling the corresponding tour and regularly checking terminating
   * conditions. Also performs some postprocessing.
   *
   * @return
   *    true if the reduction was successful, false otherwise.
   */
  bool bkz();

  /** Randomize basis between from ``min_row`` and ``max_row`` (exclusive)

      1. permute rows
      2. apply lower triangular matrix with coefficients in -1,0,1
      3. LLL reduce result

      @param min_row start in this row

      @param max_row stop at this row (exclusive)

      @param density number of non-zero coefficients in lower triangular
      transformation matrix
  **/

  void rerandomize_block(int min_row, int max_row, int density);

  /**
   * @brief Dumps the shape of the basis.
   *
   * Writes the specified prefix and shape of the current basis into the specified file.
   *
   * @param filename
   *    name of the file
   * @param prefix
   *    string to write into the file before the shape of the basis
   * @param append
   *    flag specifying if the shape should be appended to the file (or if the file
   *    should be overwritten)
   * **/
  void dump_gso(const std::string &filename, const std::string &prefix, bool append = true);

  /**
   * Status of reduction (see defs.h)
   */
  int status;

  /**
      Number of nodes visited during enumeration.
  */
  long nodes;

private:
  void print_tour(const int loop, int min_row, int max_row);
  void print_params(const BKZParam &param, ostream &out);

  bool set_status(int new_status);

  const Pruning &get_pruning(int kappa, int block_size, const BKZParam &par) const;

  // handles the general case of inserting a vector into the (dual) basis, i.e.
  // when none of the coefficients are \pm 1
  bool svp_postprocessing_generic(int kappa, int block_size, const vector<FT> &solution,
                                  bool dual = false);
  // a truncated tour: svp reducing from min_row to max_row without decreasing the window
  // size (simply returns when the last block is reduced)
  bool trunc_tour(int &kappa_max, const BKZParam &param, int min_row, int max_row);
  // a truncated dual tour: dual svp reducing from max_row to min_row without decreasing
  // the window size (simply returns when the first block is reduced)
  bool trunc_dtour(const BKZParam &param, int min_row, int max_row);

  const BKZParam &param;
  int num_rows;

  MatGSO<ZT, FT> &m;
  LLLReduction<ZT, FT> &lll_obj;
  // evaluator passed to the enumeration object to handle solutions found
  FastEvaluator<FT> evaluator;
  // slack variable for svp reductions
  FT delta;

  // an acronym for the type of block reduction used, for reporting purposes
  const char *algorithm;
  // current value of the potential function as defined in the slide reduction paper
  // used to reliably determine terminating condition during slide reduction
  FT sld_potential;

  // Temporary data
  const vector<FT> empty_target, empty_sub_tree;
  FT max_dist, delta_max_dist;
  double cputime_start;
};

/**
 * @brief Performs block reduction using BKZParam object.
 *
 * @param B
 *    basis of the lattice to be reduced
 * @param U
 *    transformation matrix (pass an empty matrix to ignore this option)
 * @param param
 *    parameter object
 * @param float_type
 *    specifies the data type used for GSO computations (see defs.h for options)
 * @param precision
 *    specifies the precision if float_type=FT_MPFR (and needs to be > 0 in that case)
 *    ignored otherwise
 * @return
 *    the status of the reduction (see defs.h for more information on the status)
 */
int bkz_reduction(IntMatrix *B, IntMatrix *U, const BKZParam &param,
                  FloatType float_type = FT_DEFAULT, int precision = 0);

/**
 * @brief Performs block reduction without transformation matrix.
 *
 * Creates a parameter object corresponding to the parameters and calls bkz_reduction
 * on it.
 *
 * @param b
 *    basis of the lattice to be reduced
 * @param block_size
 *    block_size of the reduction
 * @param flags
 *    different flags for reduction (see defs.h and bkz_param.h for more information)
 * @param float_type
 *    specifies the data type used for GSO computations (see defs.h for options)
 * @param precision
 *    specifies the precision if float_type=FT_MPFR (and needs to be > 0 in that case)
 *    ignored otherwise
 * @return
 *    the status of the reduction (see defs.h for more information on the status)
 */
int bkz_reduction(IntMatrix &b, int block_size, int flags = BKZ_DEFAULT,
                  FloatType float_type = FT_DEFAULT, int precision = 0);

/**
 * @brief Performs block reduction with transformation matrix.
 *
 * Creates a parameter object corresponding to the parameters and calls bkz_reduction
 * on it.
 *
 * @param b
 *    basis of the lattice to be reduced
 * @param u
 *    transformation matrix
 * @param block_size
 *    block_size of the reduction
 * @param flags
 *    different flags for reduction (see defs.h and bkz_param.h for more information)
 * @param float_type
 *    specifies the data type used for GSO computations (see defs.h for options)
 * @param precision
 *    specifies the precision if float_type=FT_MPFR (and needs to be > 0 in that case)
 *    ignored otherwise
 * @return
 *    the status of the reduction (see defs.h for more information on the status)
 */
int bkz_reduction(IntMatrix &b, IntMatrix &u, int block_size, int flags = BKZ_DEFAULT,
                  FloatType float_type = FT_DEFAULT, int precision = 0);

/**
 * @brief Performs HKZ reduction.
 *
 * Creates a parameter object corresponding to the parameters (and block size equal to
 * the dimension) and calls bkz_reduction on it.
 *
 * @param b
 *    basis of the lattice to be reduced
 * @param flags
 *    flags for reduction (HKZ_DEFAULT or HKZ_VERBOSE)
 * @param float_type
 *    specifies the data type used for GSO computations (see defs.h for options)
 * @param precision
 *    specifies the precision if float_type=FT_MPFR (and needs to be > 0 in that case)
 *    ignored otherwise
 * @return
 *    the status of the reduction (see defs.h for more information on the status)
 */
int hkz_reduction(IntMatrix &b, int flags = HKZ_DEFAULT, FloatType float_type = FT_DEFAULT,
                  int precision = 0);

FPLLL_END_NAMESPACE

#endif
