#include "ArgvParser.hh"

#include <string.h>

static bool prefix(const char *a, const char *b)
{
  return !strncmp(a, b, strlen(a));
}

static std::vector<std::string> Extract_Args(const char *str)
{
  std::vector<std::string> arg_list;

  const char *params = strchr(str, '=') + 1;
  char buf[strlen(params) + 1];
  const char *tok;

  strcpy(buf, params);
  tok = strtok(buf, ",");

  while (tok != nullptr) {
    arg_list.push_back(std::string(tok));
    tok = strtok(nullptr, ",");
  }

  return arg_list;
}

static std::string Extract_Single_Arg(const char *str)
{
  const char *params = strchr(str, '=') + 1;
  char buf[strlen(params) + 1];
  const char *tok;

  strcpy(buf, params);
  tok = strtok(buf, ",");

  return std::string(tok);
}

ArgvParser::ArgvParser(int argc, char **argv)
{
  DisableExternalization = false;
  DumpPasses = false;

  for (int i = 0; i < argc; i++) {
    if (!Handle_Clang_Extract_Arg(argv[i])) {
      ArgsToClang.push_back(argv[i]);
    }
  }

  Insert_Required_Parameters();
}

void ArgvParser::Insert_Required_Parameters(void)
{
  std::vector<const char *> priv_args = {
     "-fno-builtin", // clang interposes some glibc functions and then it fails to find the declaration of them.
    "-Xclang", "-detailed-preprocessing-record",
    "-Xclang", "-ast-dump",
    "-Wno-unused-variable", // Passes may instroduce unused variables later removed.
  };

  for (const char *arg : priv_args) {
    ArgsToClang.push_back(arg);
  }
}

bool ArgvParser::Handle_Clang_Extract_Arg(const char *str)
{
  if (prefix("-DCE_EXTRACT_FUNCTIONS=", str)) {
    FunctionsToExtract = Extract_Args(str);

    return true;
  }
  if (prefix("-DCE_EXPORT_SYMBOLS=", str)) {
    SymbolsToExternalize = Extract_Args(str);

    return true;
  }
  if (prefix("-DCE_OUTPUT_FILE=", str)) {
    OutputFile = Extract_Single_Arg(str);

    return true;
  }
  if (!strcmp("-DCE_NO_EXTERNALIZATION", str)) {
    DisableExternalization = true;

    return true;
  }
  if (!strcmp("-DCE_DUMP_PASSES", str)) {
    DumpPasses = true;

    return true;
  }

  return false;
}