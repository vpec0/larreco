#include "larreco/RecoAlg/DBScan3DAlg.h"
#include "lardataobj/RecoBase/SpacePoint.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

cluster::DBScan3DAlg::DBScan3DAlg(fhicl::ParameterSet const& pset)
  : epsilon(pset.get< double >("epsilon"))
  , minpts(pset.get<unsigned int>("minpts"))
{
}

//----------------------------------------------------------
cluster::DBScan3DAlg::~DBScan3DAlg()
{
}

//----------------------------------------------------------
void cluster::DBScan3DAlg::init(const std::vector<art::Ptr<recob::SpacePoint>>& sps)
{
  points.clear();
  for (auto& spt : sps){
    point_t point;
    point.sp = spt;
    points.push_back(point);
  }
}

node_t *cluster::DBScan3DAlg::create_node(unsigned int index)
{
  node_t *n = (node_t *) calloc(1, sizeof(node_t));
  if (n == NULL)
    perror("Failed to allocate node.");
  else {
    n->index = index;
    n->next = NULL;
  }
  return n;
}

int cluster::DBScan3DAlg::append_at_end(unsigned int index,
                                        epsilon_neighbours_t *en)
{
  node_t *n = create_node(index);
  if (n == NULL) {
    free(en);
    return FAILURE;
  }
  if (en->head == NULL) {
    en->head = n;
    en->tail = n;
  } else {
    en->tail->next = n;
    en->tail = n;
  }
  ++(en->num_members);
  return SUCCESS;
}

epsilon_neighbours_t *cluster::DBScan3DAlg::get_epsilon_neighbours(unsigned int index)
{
  epsilon_neighbours_t *en = (epsilon_neighbours_t *)
    calloc(1, sizeof(epsilon_neighbours_t));
  if (en == NULL) {
    perror("Failed to allocate epsilon neighbours.");
    return en;
  }
  for (unsigned int i = 0; i < points.size(); ++i) {
    if (i == index)
      continue;
    if (dist(&points[index], &points[i]) > epsilon)
      continue;
    else {
      if (append_at_end(i, en) == FAILURE) {
        destroy_epsilon_neighbours(en);
        en = NULL;
        break;
      }
    }
  }
  return en;
}

void cluster::DBScan3DAlg::destroy_epsilon_neighbours(epsilon_neighbours_t *en)
{
  if (en) {
    node_t *t, *h = en->head;
    while (h) {
      t = h->next;
      free(h);
      h = t;
    }
    free(en);
  }
}

void cluster::DBScan3DAlg::dbscan()
{
  unsigned int i, cluster_id = 0;
  for (i = 0; i < points.size(); ++i) {
    if (points[i].cluster_id == UNCLASSIFIED) {
      if (expand(i, cluster_id) == CORE_POINT)
        ++cluster_id;
    }
  }
}

int cluster::DBScan3DAlg::expand(unsigned int index,
                                 unsigned int cluster_id)
{
  int return_value = NOT_CORE_POINT;
  epsilon_neighbours_t *seeds =
    get_epsilon_neighbours(index);
  if (seeds == NULL)
    return FAILURE;

  if (seeds->num_members < minpts)
    points[index].cluster_id = NOISE;
  else {
    points[index].cluster_id = cluster_id;
    node_t *h = seeds->head;
    while (h) {
      points[h->index].cluster_id = cluster_id;
      h = h->next;
    }

    h = seeds->head;
    while (h) {
      spread(h->index, seeds, cluster_id);
      h = h->next;
    }

    return_value = CORE_POINT;
  }
  destroy_epsilon_neighbours(seeds);
  return return_value;
}

int cluster::DBScan3DAlg::spread(unsigned int index,
                                 epsilon_neighbours_t *seeds,
                                 unsigned int cluster_id)
{
  epsilon_neighbours_t *spread =
    get_epsilon_neighbours(index);
  if (spread == NULL)
    return FAILURE;
  if (spread->num_members >= minpts) {
    node_t *n = spread->head;
    point_t *d;
    while (n) {
      d = &points[n->index];
      if (d->cluster_id == NOISE ||
          d->cluster_id == UNCLASSIFIED) {
        if (d->cluster_id == UNCLASSIFIED) {
          if (append_at_end(n->index, seeds)
              == FAILURE) {
            destroy_epsilon_neighbours(spread);
            return FAILURE;
          }
        }
        d->cluster_id = cluster_id;
      }
      n = n->next;
    }
  }

  destroy_epsilon_neighbours(spread);
  return SUCCESS;
}

double cluster::DBScan3DAlg::dist(point_t *a, point_t *b)
{
  return sqrt(pow(a->sp->XYZ()[0] - b->sp->XYZ()[0], 2) +
              pow(a->sp->XYZ()[1] - b->sp->XYZ()[1], 2) +
              pow(a->sp->XYZ()[2] - b->sp->XYZ()[2], 2));
}
