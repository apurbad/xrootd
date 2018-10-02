//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Krzysztof Jamrog <krzysztof.piotr.jamrog@cern.ch>,
//         Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_OPERATION_HANDLERS_HH__
#define __XRD_CL_OPERATION_HANDLERS_HH__

#include "XrdCl/XrdClFile.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Handler allowing forwarding parameters to the next operation in pipeline
  //----------------------------------------------------------------------------
  class ForwardingHandler: public ResponseHandler
  {
      friend class PipelineHandler;

    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //------------------------------------------------------------------------
      ForwardingHandler() :
          container( new ArgsContainer() )
      {
      }

      //------------------------------------------------------------------------
      //! Callback function.
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XRootDStatus *status,
          AnyObject *response, HostList *hostList )
      {
        delete hostList;
        HandleResponse( status, response );
      }

      //------------------------------------------------------------------------
      //! Callback function.
      //------------------------------------------------------------------------
      virtual void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        delete status;
        delete response;
        delete this;
      }

      //------------------------------------------------------------------------
      //! Forward an argument to next operation in pipeline
      //!
      //! @arg    T       :  type of the value which will be saved
      //!
      //! @param  value   :  value to save
      //! @param  bucket  :  bucket in which value will be saved
      //------------------------------------------------------------------------
      template<typename T>
      void FwdArg( typename T::type value, int bucket = 1 )
      {
        container->SetArg<T>( value, bucket );
      }

    private:

      //------------------------------------------------------------------------
      //! @return : container with arguments for forwarding
      //------------------------------------------------------------------------
      std::shared_ptr<ArgsContainer>& GetArgContainer()
      {
        return container;
      }

      //------------------------------------------------------------------------
      //! container with arguments for forwarding
      //------------------------------------------------------------------------
      std::shared_ptr<ArgsContainer> container;

    protected:

      //------------------------------------------------------------------------
      //! @return  :  operation context
      //------------------------------------------------------------------------
      std::unique_ptr<OperationContext> GetOperationContext()
      {
        return std::unique_ptr<OperationContext>(
            new OperationContext( container ) );
      }
  };

  //----------------------------------------------------------------------------
  //! Helper class for checking if a given Handler is derived
  //! from ForwardingHandler
  //!
  //! @arg Hdlr : type of given handler
  //----------------------------------------------------------------------------
  template<typename Hdlr>
  struct IsForwardingHandler
  {
      //------------------------------------------------------------------------
      //! true if the Hdlr type has been derived from ForwardingHandler,
      //! false otherwise
      //------------------------------------------------------------------------
      static constexpr bool value = std::is_base_of<XrdCl::ForwardingHandler, Hdlr>::value;
  };

  //----------------------------------------------------------------------------
  //! Helper class for checking if a given Handler is derived
  //! from ForwardingHandler (overloaded for pointers)
  //!
  //! @arg Hdlr : type of given handler
  //----------------------------------------------------------------------------
  template<typename Hdlr>
  struct IsForwardingHandler<Hdlr*>
  {
      //------------------------------------------------------------------------
      //! true if the Hdlr type has been derived from ForwardingHandler,
      //! false otherwise
      //------------------------------------------------------------------------
      static constexpr bool value = std::is_base_of<XrdCl::ForwardingHandler, Hdlr>::value;
  };

  //----------------------------------------------------------------------------
  //! Handler allowing wrapping a normal ResponseHandler into
  //! a ForwaridngHandler.
  //----------------------------------------------------------------------------
  class WrappingHandler: public ForwardingHandler
  {
      friend class PipelineHandler;

    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param handler : the handler to be wrapped up
      //------------------------------------------------------------------------
      WrappingHandler( ResponseHandler *handler ) :
          handler( handler )
      {

      }

      //------------------------------------------------------------------------
      //! Callback function.
      //------------------------------------------------------------------------
      void HandleResponseWithHosts( XRootDStatus *status, AnyObject *response,
          HostList *hostList )
      {
        handler->HandleResponseWithHosts( status, response, hostList );
        delete this;
      }

      //------------------------------------------------------------------------
      //! Callback function.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        handler->HandleResponse( status, response );
        delete this;
      }

    private:

      //------------------------------------------------------------------------
      //! The wrapped handler
      //------------------------------------------------------------------------
      ResponseHandler *handler;
  };

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //----------------------------------------------------------------------------
  class SimpleFunctionWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      SimpleFunctionWrapper(
          std::function<void( XRootDStatus& )> handleFunction ) :
          fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        fun( *status );
        delete status;
        delete response;
        delete this;
      }

    private:
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XRootDStatus& )> fun;
  };

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //!
  //! @arg ResponseType : type of response returned by the server
  //----------------------------------------------------------------------------
  template<typename ResponseType>
  class FunctionWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      FunctionWrapper(
          std::function<void( XRootDStatus&, ResponseType& )> handleFunction ) :
          fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        ResponseType *res = nullptr;
        if( status->IsOK() )
          response->Get( res );
        else
          res = &nullref;
        fun( *status, *res );
        delete status;
        delete response;
        delete this;
      }

    private:
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XRootDStatus&, ResponseType& )> fun;

      //------------------------------------------------------------------------
      //! Null reference to the response (not really but acts as one)
      //------------------------------------------------------------------------
      static ResponseType nullref;
  };

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //----------------------------------------------------------------------------
  class SimpleForwardingFunctionWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      SimpleForwardingFunctionWrapper(
          std::function<void( XRootDStatus&, OperationContext& )> handleFunction ) :
          fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        auto paramsContainerWrapper = GetOperationContext();
        fun( *status, *paramsContainerWrapper.get() );
        delete status;
        delete response;
        delete this;
      }

    private:
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XRootDStatus&, OperationContext& )> fun;
  };

  //----------------------------------------------------------------------------
  // Initialize the 'null-reference'
  //----------------------------------------------------------------------------
  template<typename ResponseType>
  ResponseType FunctionWrapper<ResponseType>::nullref;

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //----------------------------------------------------------------------------
  template<typename ResponseType>
  class ForwardingFunctionWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      ForwardingFunctionWrapper(
          std::function<
              void( XRootDStatus&, ResponseType&, OperationContext& )> handleFunction ) :
          fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        ResponseType *res = nullptr;
        if( status->IsOK() )
          response->Get( res );
        else
          res = &nullref;
        auto paramsContainerWrapper = GetOperationContext();
        fun( *status, *res, *paramsContainerWrapper.get() );
        delete status;
        delete response;
        delete this;
      }

    private:
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<
          void( XRootDStatus&, ResponseType&, OperationContext &wrapper )> fun;

      //------------------------------------------------------------------------
      //! Null reference to the response (not really but acts as one)
      //------------------------------------------------------------------------
      static ResponseType nullref;
  };

  //----------------------------------------------------------------------------
  // Initialize the 'null-reference'
  //----------------------------------------------------------------------------
  template<typename ResponseType>
  ResponseType ForwardingFunctionWrapper<ResponseType>::nullref;


  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //----------------------------------------------------------------------------
  class ExOpenFuncWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      ExOpenFuncWrapper( File &f,
          std::function<void( XRootDStatus&, StatInfo& )> handleFunction ) :
          f( f ), fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        StatInfo *info = nullptr;
        if( status->IsOK() )
          f.Stat( false, info );
        else
          info = &nullref;
        fun( *status, *info );
        if( info != &nullref ) delete info;
        delete status;
        delete response;
        delete this;
      }

    private:
      File &f;
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XRootDStatus&, StatInfo& )> fun;

      //------------------------------------------------------------------------
      //! Null reference to the response (not really but acts as one)
      //------------------------------------------------------------------------
      static StatInfo nullref;
  };

  //----------------------------------------------------------------------------
  // Initialize the 'null-reference'
  //----------------------------------------------------------------------------
  StatInfo ExOpenFuncWrapper::nullref;

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //----------------------------------------------------------------------------
  class ForwardingExOpenFuncWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      ForwardingExOpenFuncWrapper( File &f,
          std::function<
              void( XRootDStatus&, StatInfo&, OperationContext& )> handleFunction ) :
          f( f ), fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        StatInfo *info = nullptr;
        if( status->IsOK() )
          f.Stat( false, info );
        else
          info = &nullref;
        auto paramsContainerWrapper = GetOperationContext();
        fun( *status, *info, *paramsContainerWrapper.get() );
        if( info != &nullref ) delete info;
        delete status;
        delete response;
        delete this;
      }

    private:
      File &f;
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XRootDStatus&, StatInfo&, OperationContext& )> fun;

      //------------------------------------------------------------------------
      //! Null reference to the response (not really but acts as one)
      //------------------------------------------------------------------------
      static StatInfo nullref;
  };

  //----------------------------------------------------------------------------
  // Initialize the 'null-reference'
  //----------------------------------------------------------------------------
  StatInfo ForwardingExOpenFuncWrapper::nullref;

  //----------------------------------------------------------------------------
  //! Pipeline exception, wrapps an XRootDStatus
  //----------------------------------------------------------------------------
  class PipelineException : public std::exception
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor from XRootDStatus
      //------------------------------------------------------------------------
      PipelineException( const XRootDStatus &error ) : error( error )
      {

      }

      //------------------------------------------------------------------------
      //! Copy constructor.
      //------------------------------------------------------------------------
      PipelineException( const PipelineException &ex ) : error( ex.error )
      {

      }

      //------------------------------------------------------------------------
      //! Assigment operator
      //------------------------------------------------------------------------
      PipelineException& operator=( const PipelineException &ex )
      {
        error = ex.error;
        return *this;
      }

      //------------------------------------------------------------------------
      //! inherited from std::exception
      //------------------------------------------------------------------------
      const char* what() const noexcept
      {
        return error.ToString().c_str();
      }

      //------------------------------------------------------------------------
      //! @return : the XRootDStatus
      //------------------------------------------------------------------------
      const XRootDStatus& GetError() const
      {
        return error;
      }

    private:

      //------------------------------------------------------------------------
      //! the XRootDStatus associated with this exception
      //------------------------------------------------------------------------
      XRootDStatus error;
  };

  //----------------------------------------------------------------------------
  //! A wrapper handler for a std::promise / std::future.
  //!
  //! @arg Response : response type
  //----------------------------------------------------------------------------
  template<typename Response>
  class FutureWrapperBase : public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor, initializes the std::future argument from its
      //! own std::promise
      //!
      //! @param ftr : the future to be linked with this handler
      //------------------------------------------------------------------------
      FutureWrapperBase( std::future<Response> &ftr ) : called( false )
      {
        ftr = prms.get_future();
      }

      //------------------------------------------------------------------------
      //! Destructor
      //!
      //! If the handler was not called sets an exception in the promise
      //------------------------------------------------------------------------
      ~FutureWrapperBase()
      {
        if( !called )
          this->SetException( XRootDStatus( stError, errPipelineFailed ) );
      }

    protected:

      //------------------------------------------------------------------------
      //! Set exception in the std::promise / std::future
      //!
      //! @param err : the error
      //------------------------------------------------------------------------
      void SetException( const XRootDStatus &err )
      {
        std::exception_ptr ex = std::make_exception_ptr( PipelineException( err ) );
        prms.set_exception( ex );
      }

      //------------------------------------------------------------------------
      //! promise that corresponds to the future
      //------------------------------------------------------------------------
      std::promise<Response> prms;

      //------------------------------------------------------------------------
      //! true if the handler has been called, false otherwise
      //------------------------------------------------------------------------
      bool called;

  };

  //----------------------------------------------------------------------------
  //! A wrapper handler for a std::promise / std::future.
  //!
  //! @arg Response : response type
  //----------------------------------------------------------------------------
  template<typename Response>
  class FutureWrapper : public FutureWrapperBase<Response>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor, @see FutureWrapperBase
      //!
      //! @param ftr : the future to be linked with this handler
      //------------------------------------------------------------------------
      FutureWrapper( std::future<Response> &ftr ) : FutureWrapperBase<Response>( ftr )
      {

      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        this->called = true;

        if( status->IsOK() )
        {
          Response *resp = 0;
          response->Get( resp );
          this->prms.set_value( std::move( *resp ) );
        }
        else
          this->SetException( *status );

        delete status;
        delete response;
        delete this;
      }
  };

  //----------------------------------------------------------------------------
  //! A wrapper handler for a std::promise / std::future, overload for void type
  //----------------------------------------------------------------------------
  template<>
  class FutureWrapper<void> : public FutureWrapperBase<void>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor, @see FutureWrapperBase
      //!
      //! @param ftr : the future to be linked with this handler
      //------------------------------------------------------------------------
      FutureWrapper( std::future<void> &ftr ) : FutureWrapperBase<void>( ftr )
      {

      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        this->called = true;


        if( status->IsOK() )
        {
          prms.set_value();
        }
        else
          SetException( *status );

        delete status;
        delete response;
        delete this;
      }
  };


  //----------------------------------------------------------------------------
  //! A base class for factories, creates ForwardingHandlers from
  //! ResponseHandler*, ResponseHandler& and std::future<Response>
  //!
  //! @arg Response : response type
  //----------------------------------------------------------------------------
  template<typename Response>
  struct RespBase
  {
      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @param h : the ResponseHandler that should be wrapped
      //! @return  : a ForwardingHandler instance
      //------------------------------------------------------------------------
      inline static ForwardingHandler* Create( ResponseHandler *hdlr )
      {
        return new WrappingHandler( hdlr );
      }

      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @param h : the ResponseHandler that should be wrapped
      //! @return  : a ForwardingHandler instance
      //------------------------------------------------------------------------
      inline static ForwardingHandler* Create( ResponseHandler &hdlr )
      {
        return new WrappingHandler( &hdlr );
      }

      //------------------------------------------------------------------------
      //! A factory method, simply forwards the given ForwardingHandler
      //!
      //! @param h : the ForwardingHandler that should be forwarded
      //! @return  : a ForwardingHandler instance
      //------------------------------------------------------------------------
      inline static ForwardingHandler* Create( ForwardingHandler *hdlr )
      {
        return hdlr;
      }

      //------------------------------------------------------------------------
      //! A factory method, simply forwards the given ForwardingHandler
      //!
      //! @param h : the ForwardingHandler that should be forwarded
      //! @return  : a ForwardingHandler instance
      //------------------------------------------------------------------------
      inline static ForwardingHandler* Create( ForwardingHandler &hdlr )
      {
        return &hdlr;
      }

      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @arg   Response : response type
      //! @param ftr      : the std::future that should be wrapped
      //------------------------------------------------------------------------
      inline static ForwardingHandler* Create( std::future<Response> &ftr )
      {
        return new FutureWrapper<Response>( ftr );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory class, creates ForwardingHandler from std::function, in addition
  //! to what RespBase provides (@see RespBase)
  //!
  //! @arg Response : response type
  //----------------------------------------------------------------------------
  template<typename Response>
  struct Resp: RespBase<Response>
  {
      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @param func : the function/functor/lambda that should be wrapped
      //! @return     : FunctionWrapper instance
      //------------------------------------------------------------------------
      inline static ForwardingHandler* Create( std::function<void( XRootDStatus&,
          Response& )> func )
      {
        return new FunctionWrapper<Response>( func );
      }

      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @param func : the function/functor/lambda that should be wrapped
      //! @return     : ForwardingFunctionWrapper instance
      //------------------------------------------------------------------------
      inline static ForwardingHandler* Create( std::function<void( XRootDStatus&,
          Response&, OperationContext& )> func )
      {
        return new ForwardingFunctionWrapper<Response>( func );
      }

      //------------------------------------------------------------------------
      //! Make the Create overloads from RespBase visible
      //------------------------------------------------------------------------
      using RespBase<Response>::Create;
  };

  //----------------------------------------------------------------------------
  //! Factory class, overloads Resp for void type
  //!
  //! @arg Response : response type
  //----------------------------------------------------------------------------
  template<>
  struct Resp<void>: RespBase<void>
  {
      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @param func : the function/functor/lambda that should be wrapped
      //! @return     : SimpleFunctionWrapper instance
      //------------------------------------------------------------------------
      inline static ForwardingHandler* Create( std::function<void( XRootDStatus& )> func )
      {
        return new SimpleFunctionWrapper( func );
      }

      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @param func : the function/functor/lambda that should be wrapped
      //! @return     : SimpleForwardingFunctionWrapper instance
      //------------------------------------------------------------------------
      inline static ForwardingHandler* Create( std::function<void( XRootDStatus&,
          OperationContext& )> func )
      {
        return new SimpleForwardingFunctionWrapper( func );
      }

      //------------------------------------------------------------------------
      //! Make the Create overloads from RespBase visible
      //------------------------------------------------------------------------
      using RespBase<void>::Create;
  };
}

#endif // __XRD_CL_OPERATIONS_HANDLERS_HH__
