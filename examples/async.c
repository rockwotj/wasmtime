/*
Example of instantiating of the WebAssembly module and invoking its exported
function.

You can compile and run this example on Linux with:

   cargo build --release -p wasmtime-c-api
   cc examples/async.c \
       -I crates/c-api/include \
       -I crates/c-api/wasm-c-api/include \
       target/release/libwasmtime.a \
       -lpthread -ldl -lm \
       -o fuel
   ./fuel

Note that on Windows and macOS the command will be similar, but you'll need
to tweak the `-lpthread` and such annotations.

You can also build using cmake:

mkdir build && cd build && cmake .. && cmake --build . --target wasmtime-async
*/

#include "wasmtime/func.h"
#include "wasmtime/linker.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wasm.h>
#include <wasmtime.h>

static void exit_with_error(const char *message, wasmtime_error_t *error,
                            wasm_trap_t *trap);

typedef struct host_print_env {
  const wasmtime_val_t* args;
  int32_t count;
} host_print_env;

wasm_trap_t *async_host_print(
    void *env, wasmtime_caller_t* c, bool* done) {
   host_print_env* async_env = (host_print_env*)env;
  printf("%d\n", async_env->args[0].of.i32);
  *done = ++async_env->count == 5;
  return NULL;
}

wasmtime_async_continuation_t *host_print(
    void *env, wasmtime_caller_t *caller, const wasmtime_val_t *args,
    size_t nargs, wasmtime_val_t *results, size_t nresults) {
  host_print_env* async_env = (host_print_env*)malloc(sizeof(host_print_env));
  async_env->args = args; 
  async_env->count = 0;
  return wasmtime_async_continuation_new(async_host_print, (void*)async_env, free);
}

int main() {
  wasmtime_error_t *error = NULL;

  wasm_config_t *config = wasm_config_new();
  assert(config != NULL);
  wasmtime_config_consume_fuel_set(config, true);
  wasmtime_config_async_support_set(config, true);

  // Create an *engine*, which is a compilation context, with our configured
  // options.
  wasm_engine_t *engine = wasm_engine_new_with_config(config);
  assert(engine != NULL);
  wasmtime_store_t *store = wasmtime_store_new(engine, NULL, NULL);
  assert(store != NULL);
  wasmtime_context_t *context = wasmtime_store_context(store);

  wasmtime_context_out_of_fuel_async_yield(context, 10, 10000);

  // Load our input file to parse it next
  FILE *file = fopen("async.wat", "r");
  if (!file) {
    printf("> Error loading file!\n");
    return 1;
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  wasm_byte_vec_t wat;
  wasm_byte_vec_new_uninitialized(&wat, file_size);
  if (fread(wat.data, file_size, 1, file) != 1) {
    printf("> Error loading module!\n");
    return 1;
  }
  fclose(file);

  // Parse the wat into the binary wasm format
  wasm_byte_vec_t wasm;
  error = wasmtime_wat2wasm(wat.data, wat.size, &wasm);
  if (error != NULL)
    exit_with_error("failed to parse wat", error, NULL);
  wasm_byte_vec_delete(&wat);

  // Compile and instantiate our module
  wasmtime_module_t *module = NULL;
  error = wasmtime_module_new(engine, (uint8_t *)wasm.data, wasm.size, &module);
  if (module == NULL)
    exit_with_error("failed to compile module", error, NULL);
  wasm_byte_vec_delete(&wasm);

  wasmtime_linker_t *linker = wasmtime_linker_new(engine);



  wasm_valtype_vec_t params;
  wasm_valtype_vec_t results;
  wasm_valtype_vec_new_uninitialized(&params, 1);
  params.data[0] = wasm_valtype_new_i32();
  wasm_valtype_vec_new_empty(&results);
  wasm_functype_t* functype = wasm_functype_new(&params, &results);

  error = wasmtime_linker_define_func_async(linker, "host", strlen("host"), "print", strlen("print"), functype, host_print, NULL, NULL);
  if (error != NULL)
    exit_with_error("failed to define async func", error, NULL);

  wasm_trap_t *trap = NULL;
  wasmtime_instance_t instance;
  error = wasmtime_linker_instantiate_async(linker, context, module, &instance,
                                            &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("failed to instantiate", error, trap);

  // Lookup our `main` export function
  wasmtime_extern_t main;
  bool ok = wasmtime_instance_export_get(context, &instance, "main",
                                         strlen("main"), &main);
  assert(ok);
  assert(main.kind == WASMTIME_EXTERN_FUNC);

  // Call it repeatedly until it fails
  uint64_t fuel_before;
  wasmtime_context_fuel_consumed(context, &fuel_before);
  wasmtime_call_future_t *fut =
      wasmtime_func_call_async(context, &main.of.func);
  printf("polling!\n");
  while (!wasmtime_call_future_poll(fut)) {
    printf("yield!\n");
  }
  error = wasmtime_call_future_get_results(fut, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("running main failed", error, trap);


  // Clean up after ourselves at this point
  wasmtime_call_future_delete(fut);
  wasmtime_module_delete(module);
  wasmtime_linker_delete(linker);
  wasmtime_store_delete(store);
  wasm_engine_delete(engine);
  return 0;
}

static void exit_with_error(const char *message, wasmtime_error_t *error,
                            wasm_trap_t *trap) {
  fprintf(stderr, "error: %s\n", message);
  wasm_byte_vec_t error_message;
  if (error != NULL) {
    wasmtime_error_message(error, &error_message);
  } else {
    wasm_trap_message(trap, &error_message);
  }
  fprintf(stderr, "%.*s\n", (int)error_message.size, error_message.data);
  wasm_byte_vec_delete(&error_message);
  exit(1);
}
