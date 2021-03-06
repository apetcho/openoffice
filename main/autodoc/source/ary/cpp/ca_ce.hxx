/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



#ifndef ARY_CPP_CA_CE_HXX
#define ARY_CPP_CA_CE_HXX

// USED SERVICES
	// BASE CLASSES
#include <ary/cpp/cp_ce.hxx>
	// OTHER
#include "cs_ce.hxx"


namespace ary
{
namespace cpp
{
    class Ce_Storage;
    class RepositoryPartition;
}
}





namespace ary
{
namespace cpp
{



/** Administrates all C++ code entities (types, operations, variables).
*/
class CeAdmin : public CePilot
{
  public:
	// LIFECYCLE
						CeAdmin(
						    RepositoryPartition &
						                        io_myReposyPartition );
    void                Set_Related(
                            const TypePilot &   i_types );
	virtual 		    ~CeAdmin();

    // INQUIRY
    const Ce_Storage &  Storage() const;

    // ACCESS
    Ce_Storage &        Storage();

    // INHERITED
    // Interface CePilot:
	virtual Namespace & CheckIn_Namespace(
                            const InputContext &
                                                i_context,
                            const String  &     i_localName );
	virtual Class &     Store_Class(
                            const InputContext &
                                                i_context,
							const String  &     i_localName,
                            E_ClassKey          i_classKey );
	virtual Enum &      Store_Enum(
                            const InputContext &
                                                i_context,
							const String  &     i_localName );
	virtual Typedef &   Store_Typedef(
                            const InputContext &
                                                i_context,
							const String  &     i_localName,
                            Type_id             i_referredType );
	virtual Function *  Store_Operation(
                            const InputContext &
                                                i_context,
							const String  &     i_localName,
                            Type_id             i_returnType,
                            const std::vector<S_Parameter> &
                                                i_parameters,
                            E_Virtuality        i_virtuality,
                            E_ConVol            i_conVol,
                            FunctionFlags       i_flags,
                            bool                i_throwExists,
                            const std::vector<Type_id> &
                                                i_exceptions );
	virtual Variable &  Store_Variable(
                            const InputContext &
                                                i_context,
							const String  &     i_localName,
                            Type_id             i_type,
                            VariableFlags       i_flags,
                            const String  &     i_arraySize,
                            const String  &     i_initValue );
	virtual EnumValue & Store_EnumValue(
                            const InputContext &
                                                i_context,
							const String  &     i_localName,
                            const String  &     i_initValue );
	virtual const Namespace &
	                    GlobalNamespace() const;
	virtual const CodeEntity &
	                    Find_Ce(
							Ce_id				i_id ) const;
	virtual const CodeEntity *
	                    Search_Ce(
							Ce_id		        i_id ) const;
    virtual const CodeEntity *
                        Search_CeAbsolute(
                            const CodeEntity &  i_curScope,
                            const QualifiedName &
                                                i_absoluteName ) const;
    virtual const CodeEntity *
                        Search_CeLocal(
                            const String  &     i_relativeName,
                            bool                i_isFunction,
                            const Namespace &   i_curNamespace,
                            const Class *       i_curClass ) const;
    virtual void        Get_QualifiedName(
                            StreamStr &         o_result,
                            const String  &     i_localName,
                            Ce_id               i_owner,
                            const char *        i_delimiter = "::" ) const;
    virtual void        Get_SignatureText(
                            StreamStr &         o_rOut,
                            const OperationSignature &
                                                i_signature,
                            const StringVector *
                                                i_sParameterNames = 0 ) const;
    virtual CesResultList
                        Search_TypeName(
                            const String  &     i_sName ) const;
   	virtual Namespace & GlobalNamespace();

  private:
    // Locals
    /// @return true, if function is duplicate.
    enum E_DuplicateFunction
    {
        df_no,
        df_replace,
        df_discard_new
    };

    /** @param o_existentFunction
            The id of the already existing function, else unset.
    */
    E_DuplicateFunction lhf_CheckAndHandle_DuplicateOperation(
                            Ce_id &             o_existentFunction,
                            const InputContext &
                                                i_context,
                            const Function &    i_newFunction );
    Namespace &         Create_Namespace(
                            Namespace &         o_parent,
                            const String  &     i_localName );
    Ce_id               Search_MatchingInstance(
                            CesResultList       i_list,
                            Ce_id               i_owner ) const;
    const TypePilot  &  Types() const;

    // DATA
    Ce_Storage          aStorage;
    const TypePilot *   pTypes;
    RepositoryPartition *
                        pCppRepositoryPartition;
};




// IMPLEMENTATION
inline const Ce_Storage &
CeAdmin::Storage() const
{
    return aStorage;
}

inline Ce_Storage &
CeAdmin::Storage()
{
    return aStorage;
}





}   // namespace cpp
}   // namespace ary
#endif
