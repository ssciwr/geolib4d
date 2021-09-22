#include "py4dgeo/py4dgeo.hpp"

#include <iostream>

namespace py4dgeo {

inline std::size_t
KDTree::Adaptor::kdtree_get_point_count() const
{
  return cloud.rows();
}

inline double
KDTree::Adaptor::kdtree_get_pt(const IndexType idx, const IndexType dim) const
{
  return cloud(idx, dim);
}

template<class BBOX>
bool
KDTree::Adaptor::kdtree_get_bbox(BBOX&) const
{
  return false;
}

inline std::size_t
KDTree::NoDistancesReturnSet::size() const
{
  return indices.size();
}

inline bool
KDTree::NoDistancesReturnSet::full() const
{
  return true;
}

inline bool
KDTree::NoDistancesReturnSet::addPoint(double dist, IndexType idx)
{
  if (dist < radius)
    indices.push_back(idx);
  return true;
}

inline double
KDTree::NoDistancesReturnSet::worstDist() const
{
  return radius;
}

KDTree::KDTree(const EigenPointCloudRef& cloud)
  : adaptor{ nullptr, cloud }
{}

KDTree::KDTree(const std::shared_ptr<EigenPointCloud>& data)
  : adaptor{ data, *data }
{}

KDTree
KDTree::create(const EigenPointCloudRef& cloud)
{
  return KDTree(cloud);
}

KDTree
KDTree::from_stream(std::istream& stream)
{
  // Read the cloud itself
  IndexType rows;
  stream.read(reinterpret_cast<char*>(&rows), sizeof(IndexType));
  auto cloud = std::make_shared<EigenPointCloud>(rows, 3);
  stream.read(reinterpret_cast<char*>(&(*cloud)(0, 0)),
              sizeof(double) * rows * 3);
  KDTree obj(cloud);

  // Read the search index
  obj.search = std::make_unique<KDTreeImpl>(
    3, obj.adaptor, nanoflann::KDTreeSingleIndexAdaptorParams(10));
  obj.search->loadIndex(stream);

  return obj;
}

std::ostream&
KDTree::to_stream(std::ostream& stream) const
{
  // Write the cloud itself. This is very unfortunate as it is a redundant
  // copy of the point cloud, but this seems to be the only way to have an
  // unpickled copy be actually usable. Scipy does exactly the same.
  IndexType rows = adaptor.cloud.rows();
  stream.write(reinterpret_cast<const char*>(&rows), sizeof(IndexType));
  stream.write(reinterpret_cast<const char*>(&adaptor.cloud(0, 0)),
               sizeof(double) * rows * 3);

  // Write the search index
  search->saveIndex(stream);

  return stream;
}

void
KDTree::build_tree(int leaf)
{
  search = std::make_unique<KDTreeImpl>(
    3, adaptor, nanoflann::KDTreeSingleIndexAdaptorParams(leaf));
  search->buildIndex();
}

void
KDTree::precompute(const EigenPointCloudRef& querypoints, double maxradius)
{
  // Resize the output data structures
  precomputed_indices.resize(querypoints.rows());
  precomputed_distances.resize(querypoints.rows());

  // Loop over query points and evaluate with maxradius
  for (IndexType i = 0; i < querypoints.rows(); ++i) {
    RadiusSearchDistanceResult result;
    radius_search_with_distances(&querypoints(i, 0), maxradius, result);

    precomputed_indices[i].resize(result.size());
    precomputed_distances[i].resize(result.size());

    for (std::size_t j = 0; j < result.size(); ++j) {
      precomputed_indices[i][j] = result[j].first;
      precomputed_distances[i][j] = result[j].second;
    }
  }
}

std::size_t
KDTree::radius_search(const double* query,
                      double radius,
                      RadiusSearchResult& result) const
{
  NoDistancesReturnSet set{ radius * radius, result };
  nanoflann::SearchParams params;
  params.sorted = false;
  return search->radiusSearchCustomCallback(query, set, params);
}

std::size_t
KDTree::radius_search_with_distances(const double* query,
                                     double radius,
                                     RadiusSearchDistanceResult& result) const
{
  nanoflann::SearchParams params;
  return search->radiusSearch(query, radius * radius, result, params);
}

std::size_t
KDTree::precomputed_radius_search(const IndexType idx,
                                  double radius,
                                  RadiusSearchResult& result) const
{
  result.clear();

  auto it = std::find_if(precomputed_distances[idx].begin(),
                         precomputed_distances[idx].end(),
                         [radius](auto d) { return d > radius * radius; });

  std::copy(precomputed_indices[idx].begin(),
            precomputed_indices[idx].begin() +
              (it - precomputed_distances[idx].begin()),
            std::back_inserter(result));
  return result.size();
}

} // namespace py4dgeo