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
//       other contributors to this software may be used to endorse or
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

#include "IECore/Group.h"
#include "IECore/Renderer.h"
#include "IECore/AttributeBlock.h"

#include "OpenEXR/ImathBoxAlgo.h"

#include "boost/format.hpp"

using namespace IECore;
using namespace std;
using namespace Imath;

const unsigned int Group::m_ioVersion = 0;
IE_CORE_DEFINEOBJECTTYPEDESCRIPTION( Group );

Group::Group()
	:	m_transform( 0 ), m_parent( 0 )
{
}

Group::~Group()
{
	clearChildren();
}

TransformPtr Group::getTransform()
{
	return m_transform;
}

ConstTransformPtr Group::getTransform() const
{
	return m_transform;
}

void Group::setTransform( TransformPtr transform )
{
	m_transform = transform;
}

Imath::M44f Group::transformMatrix( float time ) const
{
	ConstTransformPtr t = getTransform();
	if( t )
	{
		return t->transform( time );
	}
	return Imath::M44f(); // identity
}

Imath::M44f Group::globalTransformMatrix( float time ) const
{
	if( m_parent )
	{
		return m_parent->globalTransformMatrix( time ) * transformMatrix( time );
	}
	return transformMatrix( time );
}

void Group::addState( StateRenderablePtr state )
{
	if( !state )
	{
		throw InvalidArgumentException( "Cannot add null state object." );
	}
	if( state->isInstanceOf( Transform::staticTypeId() ) )
	{
		throw Exception( "Transforms cannot be added as state." );
	}
	m_state.push_back( state );
}

void Group::removeState( StateRenderablePtr state )
{
	StateContainer::iterator it = find( m_state.begin(), m_state.end(), state );
	if( it==m_state.end() )
	{
		throw Exception( "State not present in Group" );
	}
	m_state.erase( it );
}

void Group::clearState()
{
	m_state.clear();
}

const Group::StateContainer &Group::state() const
{
	return m_state;
}

void Group::addChild( VisibleRenderablePtr child )
{
	if( !child )
	{
		throw InvalidArgumentException( "Cannot add null child object." );
	}
	
	GroupPtr gChild = runTimeCast<Group>( child );
	if( gChild )
	{
		GroupPtr gChildParent = gChild->parent();
		if( gChildParent )
		{
			gChildParent->removeChild( gChild );
		}
		gChild->m_parent = this;
	}
	m_children.push_back( child );
}

void Group::removeChild( VisibleRenderablePtr child )
{
	ChildContainer::iterator it = find( m_children.begin(), m_children.end(), child );

	if( it==m_children.end() )
	{
		throw Exception( "Child is not a member of Group" );
	}

	GroupPtr gChild = runTimeCast<Group>( child );
	if( gChild )
	{
		gChild->m_parent = 0;
	}

	m_children.erase( it );
}

void Group::clearChildren()
{
	while( m_children.size() )
	{
		removeChild( m_children[0] );
	}
}

const Group::ChildContainer &Group::children() const
{
	return m_children;
}

GroupPtr Group::parent()
{
	return m_parent;
}

ConstGroupPtr Group::parent() const
{
	return m_parent;
}

void Group::copyFrom( const Object *other, CopyContext *context )
{
	VisibleRenderable::copyFrom( other, context );
	const Group *tOther = static_cast<const Group *>( other );
	if( tOther->m_transform )
	{
		m_transform = context->copy<Transform>( tOther->m_transform );
	}
	else
	{
		m_transform = 0;
	}
	clearState();
	for( StateContainer::const_iterator it=tOther->state().begin(); it!=tOther->state().end(); it++ )
	{
		addState( context->copy<StateRenderable>( *it ) );
	}
	clearChildren();
	for( ChildContainer::const_iterator it=tOther->children().begin(); it!=tOther->children().end(); it++ )
	{
		addChild( context->copy<VisibleRenderable>( *it ) );
	}
}

void Group::save( SaveContext *context ) const
{
	VisibleRenderable::save( context );
	IndexedIOInterfacePtr container = context->container( staticTypeName(), m_ioVersion );
	if( m_transform )
	{
		context->save( m_transform, container, "transform" );
	}
	container->mkdir( "state" );
	container->chdir( "state" );
		int i = 0;
		for( StateContainer::const_iterator it=state().begin(); it!=state().end(); it++ )
		{
			string name = str( boost::format( "%d" ) % i );
			context->save( *it, container, name );
			i++;
		}
	container->chdir( ".." );
	container->mkdir( "children" );
	container->chdir( "children" );
		i = 0;
		for( ChildContainer::const_iterator it = children().begin(); it!=children().end(); it++ )
		{
			string name = str( boost::format( "%d" ) % i );
			context->save( *it, container, name );
			i++;
		}
	container->chdir( ".." );
}

void Group::load( LoadContextPtr context )
{
	VisibleRenderable::load( context );
	unsigned int v = m_ioVersion;
	IndexedIOInterfacePtr container = context->container( staticTypeName(), v );
	m_transform = 0;
	try
	{
		m_transform = context->load<Transform>( container, "transform" );
	}
	catch( ... )
	{
	}
	clearState();
	container->chdir( "state" );
		IndexedIO::EntryList l = container->ls();
		for( IndexedIO::EntryList::const_iterator it=l.begin(); it!=l.end(); it++ )
		{
			addState( context->load<StateRenderable>( container, it->id() ) );
		}
	container->chdir( ".." );
	clearChildren();
	container->chdir( "children" );
		l = container->ls();
		for( IndexedIO::EntryList::const_iterator it=l.begin(); it!=l.end(); it++ )
		{
			addChild( context->load<VisibleRenderable>( container, it->id() ) );
		}
	container->chdir( ".." );
}

bool Group::isEqualTo( const Object *other ) const
{
	if( !VisibleRenderable::isEqualTo( other ) )
	{
		return false;
	}

	const Group *tOther = static_cast<const Group *>( other );

	// check transform
	if( (bool)m_transform != (bool)tOther->m_transform )
	{
		return false;
	}

	if( m_transform && !tOther->m_transform->isEqualTo( m_transform ) )
	{
		return false;
	}

	// check state
	if( m_state.size()!=tOther->m_state.size() )
	{
		return false;
	}
	for( size_t i=0; i<m_state.size(); i++ )
	{
		if( !m_state[i]->isEqualTo( tOther->m_state[i] ) )
		{
			return false;
		}
	}

	// check children
	if( m_children.size()!=tOther->m_children.size() )
	{
		return false;
	}
	for( size_t i=0; i<m_children.size(); i++ )
	{
		if( !m_children[i]->isEqualTo( tOther->m_children[i] ) )
		{
			return false;
		}
	}

	return true;
}

void Group::memoryUsage( Object::MemoryAccumulator &a ) const
{
	VisibleRenderable::memoryUsage( a );
	if( m_transform )
	{
		a.accumulate( m_transform );
	}
	for( StateContainer::const_iterator it=state().begin(); it!=state().end(); it++ )
	{
		a.accumulate( *it );
	}
	for( ChildContainer::const_iterator it=children().begin(); it!=children().end(); it++ )
	{
		a.accumulate( *it );
	}
}

void Group::render( Renderer *renderer ) const
{
	render( renderer, true );
}

void Group::render( Renderer *renderer, bool inAttributeBlock ) const
{
	AttributeBlock attributeBlock( renderer, inAttributeBlock );

	if( m_transform )
	{
		m_transform->render( renderer );
	}
	renderState( renderer );
	renderChildren( renderer );
}

void Group::renderState( Renderer *renderer ) const
{
	for( StateContainer::const_iterator it=state().begin(); it!=state().end(); it++ )
	{
		(*it)->render( renderer );
	}
}

void Group::renderChildren( Renderer *renderer ) const
{
	for( ChildContainer::const_iterator it=children().begin(); it!=children().end(); it++ )
	{
		(*it)->render( renderer );
	}
}
	
Imath::Box3f Group::bound() const
{
	Box3f result;
	for( ChildContainer::const_iterator it=children().begin(); it!=children().end(); it++ )
	{
		result.extendBy( (*it)->bound() );
	}
	return transform( result, transformMatrix() );
}