#ifndef SOLIDITY_TYPE_H_
#define SOLIDITY_TYPE_H_

#include <map>
#include <string>

#define ENUM_TO_STR(s) case s: { return #s; }

namespace SolidityTypes
{
  enum declKind // decl.getKind()
  {
    DeclVar = 0,  // clang::Decl::Var
    DeclFunction, // clang::Decl::Function
    DeclKindError
  };
  declKind get_decl_kind(const std::string& kind);
  const char* declKind_to_str(declKind the_decl);

  // counterparts of clang::Type::getTypeClass() used by get_type during conversion
  enum typeClass
  {
    TypeBuiltin = 0, // corresponds to clang::Type::Builtin
    TypeError
  };
  typeClass get_type_class(const std::string& kind);
  const char* typeClass_to_str(typeClass the_type);

  // counterparts of clang::BuiltinType::getKind() used by get_builtin_type during conversion
  enum builInTypesKind
  {
    BuiltInUChar = 0,
    BuiltInError
  };
  builInTypesKind get_builtin_type(const std::string& kind);
  const char* builInTypesKind_to_str(builInTypesKind the_blitype);

  enum storageClass
  {
    SC_None = 0,
    SC_Extern,
    SC_PrivateExtern,
    SC_Static,
    SCError
  };
  storageClass get_storage_class(const std::string& kind);
  const char* storageClass_to_str(storageClass the_strgClass);
};

#endif /* SOLIDITY_TYPE_H_ */