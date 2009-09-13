#include "libxmljs.h"
#include "natives.h"

#include "object_wrap.h"
#include "parser.h"
#include "sax_parser.h"
#include "sax_push_parser.h"

#include <v8.h>
#include <string>

using namespace v8;
using namespace libxmljs;

static void
OnFatalError (const char* location, const char* message)
{

#define FATAL_ERROR "\033[1;31mV8 FATAL ERROR.\033[m"
  if (location)
    fprintf(stderr, FATAL_ERROR " %s %s\n", location, message);
  else
    fprintf(stderr, FATAL_ERROR " %s\n", message);

  exit(1);
}

// Extracts a C str from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<str conversion failed>";
}

static void ReportException(TryCatch *try_catch) {
  Handle<Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    fprintf(stderr, "Error: (no message)\n");
    fflush(stderr);
    return;
  }
  Handle<Value> error = try_catch->Exception();
  Handle<String> stack;
  if (error->IsObject()) {
    Handle<Object> obj = Handle<Object>::Cast(error);
    Handle<Value> raw_stack = obj->Get(String::New("stack"));
    if (raw_stack->IsString()) stack = Handle<String>::Cast(raw_stack);
  }
  if (stack.IsEmpty()) {
    String::Utf8Value exception(error);

    // Print (filename):(line number): (message).
    String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber();
    fprintf(stderr, "%s:%i: %s\n", filename_string, linenum, *exception);
    // Print line of source code.
    String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = ToCString(sourceline);
    fprintf(stderr, "%s\n", sourceline_string);
    // Print wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn();
    for (int i = 0; i < start; i++) {
      fprintf(stderr, " ");
    }
    int end = message->GetEndColumn();
    for (int i = start; i < end; i++) {
      fprintf(stderr, "^");
    }
    fprintf(stderr, "\n");

    message->PrintCurrentStackTrace(stderr);


  } else {
    String::Utf8Value trace(stack);
    fprintf(stderr, "%s\n", *trace);
  }
  fflush(stderr);
}

// Executes a str within the current v8 context.
Handle<Value> ExecuteString(v8::Handle<v8::String> source,
                            v8::Handle<v8::Value> filename) {
  HandleScope scope;
  TryCatch try_catch;

  Handle<Script> script = Script::Compile(source, filename);
  if (script.IsEmpty()) {
    ReportException(&try_catch);
    exit(1);
  }

  Handle<Value> result = script->Run();
  if (result.IsEmpty()) {
    ReportException(&try_catch);
    exit(1);
  }

  return scope.Close(result);
}

static void ExecuteNativeJS(const char *filename, const char *data) {
  HandleScope scope;
  TryCatch try_catch;
  ExecuteString(String::New(data), String::New(filename));
  if (try_catch.HasCaught())  {
    puts("There is an error in Node's built-in javascript");
    puts("This should be reported as a bug!");
    ReportException(&try_catch);
    exit(1);
  }
}

void
InitializeLibXMLJS(
  v8::Handle<v8::Object> target)
{
  HandleScope scope;

  Parser::Initialize(target);
  SaxParser::Initialize(target);
  SaxPushParser::Initialize(target);

  ExecuteNativeJS("sax_parser.js", native_sax_parser);
}

// used by node.js to initialize libraries
extern "C" void
init(
  v8::Handle<v8::Object> target)
{
  HandleScope scope;
  InitializeLibXMLJS(target);
}

int
main(
  int argc,
  char* argv[])
{
  V8::SetFlagsFromCommandLine(&argc, argv, true);
  V8::Initialize();
  V8::SetFatalErrorHandler(OnFatalError);

  // Create a stack-allocated handle scope.
  HandleScope handle_scope;
  // Create a new context.
  Handle<Context> context = Context::New();
  // Enter the created context for compiling and
  // running the hello world script.
  Context::Scope context_scope(context);

  Local<Object> global_obj = Context::GetCurrent()->Global();
  Local<Object> libxml_obj = Object::New();

  InitializeLibXMLJS(libxml_obj);

  global_obj->Set(String::NewSymbol("libxml"), libxml_obj);

  // for (int i = 1; i < argc; i++) {
  //   // Create a string containing the JavaScript source code.
  //   Handle<String> source = ReadFile(argv[i]);
  //   // Compile the source code.
  //   Handle<Script> script = Script::Compile(source);
  //   // Run the script to get the result.
  //   Handle<Value> result = script->Run();
  // }

  V8::Dispose();

  return 0;
}