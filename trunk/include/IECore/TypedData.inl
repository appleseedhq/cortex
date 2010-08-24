//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2007-2010, Image Engine Design Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//	     other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

/// This file contains the implementation of TypedData. Rather than include it
/// in a public header it is #included in SimpleTypedData.cpp and VectorTypedData.cpp,
/// and the relevant template classes are explicitly instantiated there. This prevents
/// a host of problems to do with the definition of the same symbols in multiple object
/// files.

#include <cassert>

namespace IECore {

template<class T>
Object::TypeDescription<TypedData<T> > TypedData<T>::m_typeDescription;

//////////////////////////////////////////////////////////////////////////////////////
// constructors/destructors
//////////////////////////////////////////////////////////////////////////////////////

template<class T>
TypedData<T>::TypedData() : m_data( new DataHolder() )
{
}

template<class T>
TypedData<T>::TypedData(const T &data) : m_data ( new DataHolder(data) )
{
}

template<class T>
TypedData<T>::~TypedData()
{
}


//////////////////////////////////////////////////////////////////////////////////////
// object interface
//////////////////////////////////////////////////////////////////////////////////////

template <class T>
typename TypedData<T>::Ptr TypedData<T>::copy() const
{
	return staticPointerCast<TypedData<T> >( Data::copy() );
}

template <class T>
void TypedData<T>::copyFrom( const Object *other, CopyContext *context )
{
	Data::copyFrom( other, context );
	const TypedData<T> *tOther = static_cast<const TypedData<T> *>( other );
	m_data = tOther->m_data;
}

template <class T>
void TypedData<T>::save( SaveContext *context ) const
{
	Data::save( context );
	IndexedIOInterfacePtr container = context->rawContainer();
	container->write( "value", readable() );
}

template <class T>
void TypedData<T>::load( LoadContextPtr context )
{
	Data::load( context );
	try
	{
		// optimised format for new files
		IndexedIOInterfacePtr container = context->rawContainer();
		container->read( "value", writable() );
	}
	catch( ... )
	{
		// backwards compatibility with old files
		unsigned int v = 0;
		IndexedIOInterfacePtr container = context->container( staticTypeName(), v );
		container->read( "value", writable() );
	}
}

template <class T>
bool TypedData<T>::isEqualTo( const Object *other ) const
{
	if( !Data::isEqualTo( other ) )
	{
		return false;
	}
	const TypedData<T> *tOther = static_cast<const TypedData<T> *>( other );
	if( m_data==tOther->m_data )
	{
		// comparing the pointers is quick and that's good
		return true;
	}
	// pointers ain't the same - do a potentially slow comparison
	return readable()==tOther->readable();
}

//////////////////////////////////////////////////////////////////////////////////////
// data access
//////////////////////////////////////////////////////////////////////////////////////

template<class T>
void TypedData<T>::operator = (const T &data)
{
	writable() = data;
}

template<class T>
void TypedData<T>::operator = (const TypedData<T> &typedData)
{
	writable() = typedData.readable();
}

template<class T>
const T & TypedData<T>::readable() const
{
	assert( m_data );

	return m_data->data;
}

template<class T>
T & TypedData<T>::writable()
{
	assert( m_data );
	if (m_data->refCount() > 1)
	{
		// duplicate the data

		m_data = new DataHolder(m_data->data);
	}
	return m_data->data;
}

template<class T>
void TypedData<T>::memoryUsage( Object::MemoryAccumulator &accumulator ) const
{
	Data::memoryUsage( accumulator );
	accumulator.accumulate( &readable(), sizeof( T ) );
}

//////////////////////////////////////////////////////////////////////////////////////
// low level data access
//////////////////////////////////////////////////////////////////////////////////////

template <class T>
bool TypedData<T>::hasBase()
{
	return TypedDataTraits< TypedData<T> >::HasBase::value;
}

template <class T>
size_t TypedData<T>::baseSize() const
{
	if ( !TypedData<T>::hasBase() )
	{
		throw Exception( std::string( TypedData<T>::staticTypeName() ) + " has no base type." );
	}
	return ( sizeof( T ) / sizeof( typename TypedData<T>::BaseType ) );
}

template <class T>
const typename TypedData<T>::BaseType *TypedData<T>::baseReadable() const
{
	if ( !TypedData<T>::hasBase() )
	{
		throw Exception( std::string( TypedData<T>::staticTypeName() ) + " has no base type." );
	}
	return reinterpret_cast< const typename TypedData<T>::BaseType * >( &readable() );
}

template <class T>
typename TypedData<T>::BaseType *TypedData<T>::baseWritable()
{
	if ( !TypedData<T>::hasBase() )
	{
		throw Exception( std::string( TypedData<T>::staticTypeName() ) + " has no base type." );
	}
	return reinterpret_cast< typename TypedData<T>::BaseType * >( &writable() );
}

//////////////////////////////////////////////////////////////////////////////////////
// macros for TypedData function specializations
//////////////////////////////////////////////////////////////////////////////////////

#define IE_CORE_DEFINECOMMONTYPEDDATASPECIALISATION( TNAME, TID ) \
	IECORE_RUNTIMETYPED_DEFINETEMPLATESPECIALISATION( TNAME, TID )

#define IE_CORE_DEFINETYPEDDATANOBASESIZE( TNAME )							\
	template <>																\
	size_t TNAME::baseSize() const									\
	{																		\
		throw Exception( std::string( TNAME::staticTypeName() ) + " has no base type." );	\
	}																		\

#define IE_CORE_DEFINEBASETYPEDDATAIOSPECIALISATION( TNAME, N )										\
																									\
	template<>																						\
	void TNAME::save( SaveContext *context ) const													\
	{																								\
		Data::save( context );																		\
		assert( baseSize() == N );																	\
		IndexedIOInterfacePtr container = context->rawContainer();									\
		container->write( "value", TNAME::baseReadable(), TNAME::baseSize() );						\
	}																								\
																									\
	template<>																						\
	void TNAME::load( LoadContextPtr context )														\
	{																								\
		Data::load( context );																		\
		assert( ( sizeof( TNAME::ValueType ) / sizeof( TNAME::BaseType ) ) == N );					\
		IndexedIOInterfacePtr container;															\
		TNAME::BaseType *p = TNAME::baseWritable();													\
		try																							\
		{																							\
			container = context->rawContainer();													\
			container->read( "value", p, N );														\
		}																							\
		catch( ... )																				\
		{																							\
			unsigned int v = 0;																		\
			container = context->container( staticTypeName(), v );									\
			container->read( "value", p, N );														\
		}																							\
	}																								\


} // namespace IECore
