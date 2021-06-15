#include <solidity-ast-frontend/solidity_convert.h>
#include <util/arith_tools.h>
#include <util/bitvector.h>
#include <util/c_types.h>
#include <util/expr_util.h>
#include <util/i2string.h>
#include <util/mp_arith.h>
#include <util/std_code.h>
#include <util/std_expr.h>
#include <iomanip>

solidity_convertert::solidity_convertert(contextt &_context, nlohmann::json &_ast_json):
  context(_context),
  ast_json(_ast_json)
{
}

bool solidity_convertert::convert()
{
  // This method convert each declarations in ast_json to symbols and add them to the context.

  if (!ast_json.contains("nodes")) // check json file contains AST nodes as Solidity might change
    assert(!"JSON file does not contain any AST nodes");

  // By now the context should have the symbols of all ESBMC's intrinsics and the dummy main
  // We need to convert Solidity AST nodes to the equivalent symbols and add them to the context
  nlohmann::json nodes = ast_json["nodes"];
  unsigned index = 0;
  nlohmann::json::iterator itr = nodes.begin();
  for (; itr != nodes.end(); ++itr, ++index)
  {
    std::string node_type = (*itr)["nodeType"].get<std::string>();
    if (node_type == "ContractDefinition") // contains AST nodes we need
    {
      if(convert_ast_nodes(*itr))
        return true; // 'true' indicates something goes wrong.
    }
  }

  assert(!"all symbols done?");

  return false; // 'false' indicates successful completion.
}

bool solidity_convertert::convert_ast_nodes(nlohmann::json &contract_def)
{
  unsigned index = 0;
  nlohmann::json ast_nodes = contract_def["nodes"];
  nlohmann::json::iterator itr = ast_nodes.begin();
  for (; itr != ast_nodes.end(); ++itr, ++index)
  {
    nlohmann::json ast_node = *itr;
    std::string node_name = ast_node["name"].get<std::string>();
    std::string node_type = ast_node["nodeType"].get<std::string>();
    printf("@@ Converting node[%u]: name=%s, nodeType=%s ...\n",
        index, node_name.c_str(), node_type.c_str());
    //print_json_array_element(ast_node, node_type, index);
    exprt dummy_decl;
    if(get_decl(ast_node, dummy_decl))
      return true;
  }

  return false;
}

// This method convert declarations. They are called when those declarations
// are to be added to the context. If a variable or function is being called
// but then get_decl_expr is called instead
bool solidity_convertert::get_decl(nlohmann::json &ast_node, exprt &new_expr)
{
  new_expr = code_skipt();

  if (!ast_node.contains("nodeType"))
  {
    assert(!"Missing \'nodeType\' filed in ast_node");
  }

  SolidityTypes::declKind decl_kind =
    SolidityTypes::get_decl_kind(static_cast<std::string>(ast_node.at("nodeType")));
  assert(decl_kind != SolidityTypes::DeclKindError);

  switch(decl_kind)
  {
    case SolidityTypes::declKind::DeclVar:
    {
      auto vd_tracker = std::make_shared<VarDeclTracker>(ast_node); // VariableDeclaration tracker
      vd_tracker->config();
      return get_var(vd_tracker, new_expr);
    }
    default:
      std::cerr << "**** ERROR: ";
      std::cerr << "Unrecognized / unimplemented declaration in Solidity AST : "
                << SolidityTypes::declKind_to_str(decl_kind)
                << std::endl;
      return true;
  }

  assert(!"get_decl done?");
  return false;
}

bool solidity_convertert::get_var(varDeclTrackerPtr vd, exprt &new_expr)
{
  // Get type
  typet t;
  if(get_type(vd->get_qualtype_tracker(), t))
    return true;

  assert(!vd->get_hasAttrs() && "expecting false hasAttrs but got true"); // to be extended in the future

  std::string id, name;
  get_decl_name(vd->get_nameddecl_tracker(), name, id);

  assert(!"get_var done?");
  return false;
}

static std::string get_decl_name(const NamedDeclTracker &nd)
{
  if (nd.get_hasIdentifier())
  {
    return nd.get_name();
  }
  else
  {
    assert(!"Unsupported - nd.get_hasIdentifier() returns false");
    return "Error"; // just to make come old gcc compiler happy
  }
  return "Error"; // just to make come old gcc compiler happy
}

void solidity_convertert::get_decl_name(
  const NamedDeclTracker &nd,
  std::string &name,
  std::string &id)
{
  id = name = ::get_decl_name(nd);

  switch(nd.get_named_decl_kind()) // it's our declClass in Solidity! same as being used in get_decl
  {
    case SolidityTypes::declKind::DeclVar:
    {
      if(name.empty())
      {
        assert(!"unsupported - name is empty in get_decl_name for DeclVar");
      }
      break;
    }

    default:
      if(name.empty()) // print name gives the function name when this is part of get_function back traces
      {
        assert(!"Declaration has empty name - unsupported named_decl_kind");;
      }
  }

  // get DeclUSR to be used in C context
  if(!generate_decl_usr(nd, name, id))
    return;

  // Otherwise, abort
  assert(!"Unable to generate USR");
  abort();
}

// Auxiliary function to mimic clang's clang::index::generateUSRForDecl()
bool solidity_convertert::generate_decl_usr(
  const NamedDeclTracker &nd,
  std::string &name,
  std::string &id)
{
  switch(nd.get_named_decl_kind()) // it's our declClass in Solidity! same as being used in get_decl
  {
    case SolidityTypes::declKind::DeclVar:
    {
      std::string id_prefix = "c:@";
      id = id_prefix + name;
      return false;
    }

    default:
      assert(!"unsupported named_decl_kind when generating declUSR");
  }

  return true;
}

bool solidity_convertert::get_type(
  const QualTypeTracker &q_type,
  typet &new_type)
{
  if(get_sub_type(q_type, new_type))
    return true;

  if(q_type.get_isConstQualified()) // TODO: what's Solidity's equivalent?
    new_type.cmt_constant(true);

  if(q_type.get_isVolatileQualified())
    new_type.cmt_volatile(true);

  if(q_type.get_isRestrictQualified())
    new_type.restricted(true);

  return false;
}

bool solidity_convertert::get_sub_type(const QualTypeTracker &q_type, typet &new_type)
{
  switch(q_type.get_type_class())
  {
    case SolidityTypes::typeClass::TypeBuiltin:
    {
      if(get_builtin_type(q_type, new_type))
        return true;
      break;
    }
    default:
      std::cerr << "Conversion of unsupported node qual type: \"";
      std::cerr << SolidityTypes::typeClass_to_str(q_type.get_type_class()) << std::endl;
      return true;
  }

  return false;
}

bool solidity_convertert::get_builtin_type(
  const QualTypeTracker &q_type,
  typet &new_type)
{
  std::string c_type;

  switch(q_type.get_bt_kind())
  {
    case SolidityTypes::builInTypesKind::BuiltInUChar:
    {
      new_type = unsigned_char_type();
      c_type = "unsigned_char";
      break;
    }
    default:
      std::cerr << "Unrecognized clang builtin type "
                << SolidityTypes::builInTypesKind_to_str(q_type.get_bt_kind())
                << std::endl;
      return true;
  }

  new_type.set("#cpp_type", c_type);
  return false;
}

void solidity_convertert::print_json_element(nlohmann::json &json_in, const unsigned index,
    const std::string &key, const std::string& json_name)
{
  printf("### %s element[%u] content: key=%s, size=%lu ###\n",
      json_name.c_str(), index, key.c_str(), json_in.size());
  std::cout << std::setw(2) << json_in << '\n'; // '2' means 2x indentations in front of each line
  printf("\n");
}

void solidity_convertert::print_json_array_element(nlohmann::json &json_in,
    const std::string& node_type, const unsigned index)
{
  printf("### node[%u]: nodeType=%s ###\n", index, node_type.c_str());
  std::cout << std::setw(2) << json_in << '\n'; // '2' means 2x indentations in front of each line
  printf("\n");
}