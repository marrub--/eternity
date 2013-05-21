// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 David Hill, James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//    Base for classes with custom RTTI. Adapted from the original ThinkerType.
//
//-----------------------------------------------------------------------------

#ifndef E_RTTI_H__
#define E_RTTI_H__

#include "z_zone.h"

//
// RTTIObject
//
// Base class for all ZoneObject descendants that additionally desire to use
// fast, efficient, portable run-time type identification.
//
class RTTIObject : public ZoneObject
{
public:
   // RTTI Proxy Type
   // This acts as the ultimate base class of all other proxy types.
   class Type
   {
   private:
      enum { NUMTYPECHAINS = 31 };

      // addType is invoked by the constructor and places the type into a
      // global hash table for lookups by class name.
      void addType();

      Type *next; // next type on the same hash chain

      static Type *rttiTypes[NUMTYPECHAINS]; // hash table

   protected:
      // The constructor is always protected, but the type being proxied is
      // always a friend so that it can construct a singleton instance of the
      // proxy type as a static class member.
      Type(const char *pName, Type *pParent)
        : parent(pParent), name(pName)
      {
         addType();
      }

      Type       *const parent; // Pointer to the parent class's proxy instance
      const char *const name;   // Name of this class

   public:
      typedef RTTIObject Class; // Type of the class being proxied
      friend class RTTIObject;  // The proxied class is always a friend.

      //
      // isAncestorOf
      //
      // Returns true if:
      // * This proxy represents the actual type referred to by "type".
      // * This proxy represents a super class of the type referred to by "type".
      //
      bool isAncestorOf(const Type *type) const
      {
         while(type)
         {
            if(type == this)
               return true;
            type = type->parent;
         }
         return false;
      }

      static Type *FindType(const char *pName);

      template<typename T> static T *FindType(const char *pName)
      {
         Type *type = FindType(pName);

         if(!T::Class::StaticType.isAncestorOf(type))
            return NULL;

         return static_cast<T *>(type);
      }

      // virtual constructor factory method
      virtual RTTIObject *newObject() const { return new RTTIObject; }
   };

   // RTTIObject's proxy type instance.
   static Type StaticType;

   // getDynamicType will always return the most-derived (or "actual") type of
   // the object even when invoked through pointers or references to super
   // classes. You are required to override this method.
   virtual const Type *getDynamicType() const { return &StaticType; }

   // getClassName will always return the name of the most-derived (or "actual")
   // type of the object even when invoked through pointers or references to
   // super classes.
   const char *getClassName() const { return getDynamicType()->name; }

   //
   // isInstanceOf
   //
   // Returns true *only* if "type" represents the actual type of this object.
   //
   bool isInstanceOf(const Type *type) const
   {
      return (getDynamicType() == type);
   }

   //
   // isInstanceOf(const char *className)
   //
   // Returns true if the object's actual type matches the passed-in name.
   //
   bool isInstanceOf(const char *className) const
   {
      return !strcmp(getClassName(), className);
   }

   //
   // isAncestorOf
   //
   // Returns true if "type" represents the actual type of this object, or
   // a type which is a descendant type.
   //
   bool isAncestorOf(const Type *type) const
   {
      return getDynamicType()->isAncestorOf(type);
   }

   //
   // isDescendantOf
   //
   // Returns true if "type" represents the actual type of this object, or
   // a type which is an ancestral type.
   //
   bool isDescendantOf(const Type *type) const
   {
      return type->isAncestorOf(getDynamicType());
   }

   //
   // Forwarding statics for Type class utilities
   //

   static Type *FindType(const char *pName)
   {
      return Type::FindType(pName);
   }

   template<typename T> T *FindType(const char *pName)
   {
      return Type::FindType<T>(pName);
   }

   template<typename T> typename T::Type *FindTypeCls(const char *pName)
   {
      return Type::FindType<typename T::Type>(pName);
   }
};

//
// DECLARE_RTTI_TYPE
//
// Use this macro once per RTTIObject descendant, inside the class definition.
// The following public members are declared:
//
// typedef inherited Super;
// * This allows name::Super to be used as a type which implicitly references
//   the immediate parent class.
//
// class Type;
// * This is the class's RTTI proxy type and it automatically inherits from the
//   Super class's proxy. The constructor is protected. The proxy class exposes
//   the type it proxies for (ie., name) as Type::Class, and a virtual
//   newObject() factory constructor method. Note using this macro will exact
//   the requirement of a default constructor on the RTTIObject descendant.
//
// static Type StaticType;
// * This is the singleton instance of RTTI proxy type for the RTTIObject
//   descendant. It is instantiated by the IMPLEMENT_RTTI_TYPE macro below.
//
// virtual const Type *getDynamicType() const;
// * This method of the RTTIObject descendant will return the StaticType
//   member, which in the context of each individual class, is the instance
//   representing the actual most-derived type of the object, ie.,
//   name::StaticType (the parent instances of StaticType are progressively
//   hidden in each descendant scope).
//

#define DECLARE_RTTI_TYPE(name, inherited) \
public: \
   typedef inherited Super; \
   class Type : public Super::Type \
   { \
   protected: \
      Type(char const *pName, Super::Type *pParent) \
       : Super::Type( pName, pParent ) {} \
   public: \
      typedef name Class; \
      friend class name; \
      virtual name *newObject() const { return new name ; } \
   }; \
   static Type StaticType; \
   virtual const Type *getDynamicType() const { return &StaticType; } \
private:

//
// IMPLEMENT_RTTI_TYPE
//
// Use this macro once per RTTIObject descendant, at file scope within a single
// relevant translation module.
//
// Example:
//   IMPLEMENT_RTTI_TYPE(FireFlickerThinker)
//   This defines FireFlickerThinker::StaticType and constructs it with
//   "FireFlickerThinker" as its class name.
//

#define IMPLEMENT_RTTI_TYPE(name) \
name::Type name::StaticType(#name, &Super::StaticType);

#define RTTI(cls) (&cls::StaticType)

//
// runtime_cast
//
// This is the most general equivalent of dynamic_cast which uses the custom
// RTTI system instead of C++'s built-in typeid structures.
//
template<typename T> inline T runtime_cast(RTTIObject *robj)
{
   typedef typename eeprestd::remove_pointer<T>::type base_type;

   return (robj && robj->isDescendantOf(&base_type::StaticType)) ?
      static_cast<T>(robj) : NULL;
}

#endif //E_RTTI_H__

// EOF
