// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "quadi.h"
#include "quad_intersector_moeller.h"
#include "quad_intersector_pluecker.h"

namespace embree
{
  namespace isa
  {
    /*! Intersects M quads with 1 ray */
    template<int M, bool filter>
      struct QuadMiIntersector1Moeller
    {
      typedef QuadMi<M> Primitive;
      typedef QuadMIntersector1MoellerTrumbore<M,filter> Precalculations;
        
      /*! Intersect a ray with the M quads and updates the hit. */
      static __forceinline void intersect(const Precalculations& pre, Ray& ray, IntersectContext* context, const Primitive& quad)
      {
        STAT3(normal.trav_prims,1,1,1);
        Vec3<vfloat<M>> v0,v1,v2,v3; quad.gather(v0,v1,v2,v3,context->scene);
        pre.intersect(ray,context,v0,v1,v2,v3,quad.geomIDs,quad.primIDs); 
      }
        
      /*! Test if the ray is occluded by one of M quads. */
      static __forceinline bool occluded(const Precalculations& pre, Ray& ray, IntersectContext* context, const Primitive& quad)
      {
        STAT3(shadow.trav_prims,1,1,1);
        Vec3<vfloat<M>> v0,v1,v2,v3; quad.gather(v0,v1,v2,v3,context->scene);
        return pre.occluded(ray,context,v0,v1,v2,v3,quad.geomIDs,quad.primIDs);
      }

      /*! Intersect an array of rays with an array of M primitives. */
      static __forceinline size_t intersect(Precalculations* pre, size_t valid, Ray** rays, IntersectContext* context,  size_t ty, const Primitive* prim, size_t num)
      {
        size_t valid_isec = 0;
        do {
          const size_t i = __bscf(valid);
          const float old_far = rays[i]->tfar;
          for (size_t n=0; n<num; n++)
            intersect(pre[i],*rays[i],context,prim[n]);
          valid_isec |= (rays[i]->tfar < old_far) ? ((size_t)1 << i) : 0;            
        } while(unlikely(valid));
        return valid_isec;
      }
    };

    /*! Intersects M triangles with K rays. */
    template<int M, int K, bool filter>
      struct QuadMiIntersectorKMoeller
      {
        typedef QuadMi<M> Primitive;
        typedef QuadMIntersectorKMoellerTrumbore<M,K,filter> Precalculations;
        
        /*! Intersects K rays with M triangles. */
        static __forceinline void intersect(const vbool<K>& valid_i, Precalculations& pre, RayK<K>& ray, IntersectContext* context, const QuadMi<M>& quad)
        {
          Scene* scene = context->scene;
          for (size_t i=0; i<QuadMi<M>::max_size(); i++)
          {
            if (!quad.valid(i)) break;
            STAT3(normal.trav_prims,1,popcnt(valid_i),K);
            const Vec3fa & _p0 = quad.getVertex(quad.v0,i,scene);
            const Vec3fa & _p1 = quad.getVertex(quad.v1,i,scene);
            const Vec3fa & _p2 = quad.getVertex(quad.v2,i,scene);
            const Vec3fa & _p3 = quad.getVertex(quad.v3,i,scene);            
            const Vec3<vfloat<K>> p0(_p0.x,_p0.y,_p0.z);
            const Vec3<vfloat<K>> p1(_p1.x,_p1.y,_p1.z);
            const Vec3<vfloat<K>> p2(_p2.x,_p2.y,_p2.z);
            const Vec3<vfloat<K>> p3(_p3.x,_p3.y,_p3.z);
            pre.intersectK(valid_i,ray,p0,p1,p2,p3,IntersectKEpilogM<M,K,filter>(ray,context,quad.geomIDs,quad.primIDs,i));
          }
        }
        
        /*! Test for K rays if they are occluded by any of the M triangles. */
        static __forceinline vbool<K> occluded(const vbool<K>& valid_i, Precalculations& pre, RayK<K>& ray, IntersectContext* context, const QuadMi<M>& quad)
        {
          Scene* scene = context->scene;
          vbool<K> valid0 = valid_i;
          for (size_t i=0; i<QuadMi<M>::max_size(); i++)
          {
            if (!quad.valid(i)) break;
            STAT3(shadow.trav_prims,1,popcnt(valid0),K);
            const Vec3fa & _p0 = quad.getVertex(quad.v0,i,scene);
            const Vec3fa & _p1 = quad.getVertex(quad.v1,i,scene);
            const Vec3fa & _p2 = quad.getVertex(quad.v2,i,scene);
            const Vec3fa & _p3 = quad.getVertex(quad.v3,i,scene);            
            const Vec3<vfloat<K>> p0(_p0.x,_p0.y,_p0.z);
            const Vec3<vfloat<K>> p1(_p1.x,_p1.y,_p1.z);
            const Vec3<vfloat<K>> p2(_p2.x,_p2.y,_p2.z);
            const Vec3<vfloat<K>> p3(_p3.x,_p3.y,_p3.z);
            if (pre.intersectK(valid0,ray,p0,p1,p2,p3,OccludedKEpilogM<M,K,filter>(valid0,ray,context,quad.geomIDs,quad.primIDs,i)))
              break;
          }
          return !valid0;
        }
        
        /*! Intersect a ray with M triangles and updates the hit. */
        static __forceinline void intersect(Precalculations& pre, RayK<K>& ray, size_t k, IntersectContext* context, const QuadMi<M>& quad)
        {
          STAT3(normal.trav_prims,1,1,1);
          Vec3vf4 v0,v1,v2,v3; quad.gather(v0,v1,v2,v3,context->scene);
          pre.intersect1(ray,k,context,v0,v1,v2,v3,quad.geomIDs,quad.primIDs); 
        }
        
        /*! Test if the ray is occluded by one of the M triangles. */
        static __forceinline bool occluded(Precalculations& pre, RayK<K>& ray, size_t k, IntersectContext* context, const QuadMi<M>& quad)
        {
          STAT3(shadow.trav_prims,1,1,1);
          Vec3vf4 v0,v1,v2,v3; quad.gather(v0,v1,v2,v3,context->scene);
          return pre.occluded1(ray,k,context,v0,v1,v2,v3,quad.geomIDs,quad.primIDs); 
        }
      };

    /*! Intersects M quads with 1 ray */
    template<int M, bool filter>
      struct QuadMiIntersector1Pluecker
    {
      typedef QuadMi<M> Primitive;
      typedef QuadMIntersector1Pluecker<M,filter> Precalculations;
        
      /*! Intersect a ray with the M quads and updates the hit. */
      static __forceinline void intersect(const Precalculations& pre, Ray& ray, IntersectContext* context, const Primitive& quad)
      {
        STAT3(normal.trav_prims,1,1,1);
        Vec3<vfloat<M>> v0,v1,v2,v3; quad.gather(v0,v1,v2,v3,context->scene);
        pre.intersect(ray,context,v0,v1,v2,v3,quad.geomIDs,quad.primIDs); 
      }
        
      /*! Test if the ray is occluded by one of M quads. */
      static __forceinline bool occluded(const Precalculations& pre, Ray& ray, IntersectContext* context, const Primitive& quad)
      {
        STAT3(shadow.trav_prims,1,1,1);
        Vec3<vfloat<M>> v0,v1,v2,v3; quad.gather(v0,v1,v2,v3,context->scene);
        return pre.occluded(ray,context,v0,v1,v2,v3,quad.geomIDs,quad.primIDs);
      }

      /*! Intersect an array of rays with an array of M primitives. */
      static __forceinline size_t intersect(Precalculations* pre, size_t valid, Ray** rays, IntersectContext* context,  size_t ty, const Primitive* prim, size_t num)
      {
        size_t valid_isec = 0;
        do {
          const size_t i = __bscf(valid);
          const float old_far = rays[i]->tfar;
          for (size_t n=0; n<num; n++)
            intersect(pre[i],*rays[i],context,prim[n]);
          valid_isec |= (rays[i]->tfar < old_far) ? ((size_t)1 << i) : 0;            
        } while(unlikely(valid));
        return valid_isec;
      }
    };

    /*! Intersects M triangles with K rays. */
    template<int M, int K, bool filter>
      struct QuadMiIntersectorKPluecker
      {
        typedef QuadMi<M> Primitive;
        typedef QuadMIntersectorKPluecker<M,K,filter> Precalculations;
        
        /*! Intersects K rays with M triangles. */
        static __forceinline void intersect(const vbool<K>& valid_i, Precalculations& pre, RayK<K>& ray, IntersectContext* context, const QuadMi<M>& quad)
        {
          Scene* scene = context->scene;
          for (size_t i=0; i<QuadMi<M>::max_size(); i++)
          {
            if (!quad.valid(i)) break;
            STAT3(normal.trav_prims,1,popcnt(valid_i),K);
            const Vec3fa & _p0 = quad.getVertex(quad.v0,i,scene);
            const Vec3fa & _p1 = quad.getVertex(quad.v1,i,scene);
            const Vec3fa & _p2 = quad.getVertex(quad.v2,i,scene);
            const Vec3fa & _p3 = quad.getVertex(quad.v3,i,scene);            
            const Vec3<vfloat<K>> p0(_p0.x,_p0.y,_p0.z);
            const Vec3<vfloat<K>> p1(_p1.x,_p1.y,_p1.z);
            const Vec3<vfloat<K>> p2(_p2.x,_p2.y,_p2.z);
            const Vec3<vfloat<K>> p3(_p3.x,_p3.y,_p3.z);
            pre.intersectK(valid_i,ray,p0,p1,p2,p3,IntersectKEpilogM<M,K,filter>(ray,context,quad.geomIDs,quad.primIDs,i));
          }
        }
        
        /*! Test for K rays if they are occluded by any of the M triangles. */
        static __forceinline vbool<K> occluded(const vbool<K>& valid_i, Precalculations& pre, RayK<K>& ray, IntersectContext* context, const QuadMi<M>& quad)
        {
          Scene* scene = context->scene;
          vbool<K> valid0 = valid_i;
          for (size_t i=0; i<QuadMi<M>::max_size(); i++)
          {
            if (!quad.valid(i)) break;
            STAT3(shadow.trav_prims,1,popcnt(valid0),K);
            const Vec3fa & _p0 = quad.getVertex(quad.v0,i,scene);
            const Vec3fa & _p1 = quad.getVertex(quad.v1,i,scene);
            const Vec3fa & _p2 = quad.getVertex(quad.v2,i,scene);
            const Vec3fa & _p3 = quad.getVertex(quad.v3,i,scene);            
            const Vec3<vfloat<K>> p0(_p0.x,_p0.y,_p0.z);
            const Vec3<vfloat<K>> p1(_p1.x,_p1.y,_p1.z);
            const Vec3<vfloat<K>> p2(_p2.x,_p2.y,_p2.z);
            const Vec3<vfloat<K>> p3(_p3.x,_p3.y,_p3.z);
            if (pre.intersectK(valid0,ray,p0,p1,p2,p3,OccludedKEpilogM<M,K,filter>(valid0,ray,context,quad.geomIDs,quad.primIDs,i)))
              break;
          }
          return !valid0;
        }
        
        /*! Intersect a ray with M triangles and updates the hit. */
        static __forceinline void intersect(Precalculations& pre, RayK<K>& ray, size_t k, IntersectContext* context, const QuadMi<M>& quad)
        {
          STAT3(normal.trav_prims,1,1,1);
          Vec3vf4 v0,v1,v2,v3; quad.gather(v0,v1,v2,v3,context->scene);
          pre.intersect1(ray,k,context,v0,v1,v2,v3,quad.geomIDs,quad.primIDs); 
        }
        
        /*! Test if the ray is occluded by one of the M triangles. */
        static __forceinline bool occluded(Precalculations& pre, RayK<K>& ray, size_t k, IntersectContext* context, const QuadMi<M>& quad)
        {
          STAT3(shadow.trav_prims,1,1,1);
          Vec3vf4 v0,v1,v2,v3; quad.gather(v0,v1,v2,v3,context->scene);
          return pre.occluded1(ray,k,context,v0,v1,v2,v3,quad.geomIDs,quad.primIDs); 
        }
      };
  }
}

