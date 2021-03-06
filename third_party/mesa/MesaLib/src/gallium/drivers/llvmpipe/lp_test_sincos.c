/**************************************************************************
 *
 * Copyright 2010 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/


#include <stdlib.h>
#include <stdio.h>

#include "gallivm/lp_bld.h"
#include "gallivm/lp_bld_init.h"
#include "gallivm/lp_bld_arit.h"
#include "util/u_pointer.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/Scalar.h>

#include "lp_test.h"


void
write_tsv_header(FILE *fp)
{
   fprintf(fp,
           "result\t"
           "format\n");

   fflush(fp);
}


#ifdef PIPE_ARCH_SSE

#define USE_SSE2
#include "sse_mathfun.h"

typedef __m128 (*test_sincos_t)(__m128);

static LLVMValueRef
add_sincos_test(LLVMModuleRef module, boolean sin)
{
   LLVMTypeRef v4sf = LLVMVectorType(LLVMFloatType(), 4);
   LLVMTypeRef args[1] = { v4sf };
   LLVMValueRef func = LLVMAddFunction(module, "sincos", LLVMFunctionType(v4sf, args, 1, 0));
   LLVMValueRef arg1 = LLVMGetParam(func, 0);
   LLVMBuilderRef builder = LLVMCreateBuilder();
   LLVMBasicBlockRef block = LLVMAppendBasicBlock(func, "entry");
   LLVMValueRef ret;
   struct lp_build_context bld;

   bld.builder = builder;
   bld.type.floating = 1;
   bld.type.width = 32;
   bld.type.length = 4;

   LLVMSetFunctionCallConv(func, LLVMCCallConv);

   LLVMPositionBuilderAtEnd(builder, block);
   ret = sin ? lp_build_sin(&bld, arg1) : lp_build_cos(&bld, arg1);
   LLVMBuildRet(builder, ret);
   LLVMDisposeBuilder(builder);
   return func;
}

static void
printv(char* string, v4sf value)
{
   v4sf v = value;
   uint32_t  *p = (uint32_t *) &v;
   float *f = (float *)&v;
   printf("%s: %f(%x) %f(%x) %f(%x) %f(%x)\n", string,
           f[0], p[0], f[1], p[1], f[2], p[2], f[3], p[3]);
}

PIPE_ALIGN_STACK
static boolean
test_sincos(unsigned verbose, FILE *fp)
{
   LLVMModuleRef module = NULL;
   LLVMValueRef test_sin = NULL, test_cos = NULL;
   LLVMExecutionEngineRef engine = lp_build_engine;
   LLVMPassManagerRef pass = NULL;
   char *error = NULL;
   test_sincos_t sin_func;
   test_sincos_t cos_func;
   float unpacked[4];
   boolean success = TRUE;

   module = LLVMModuleCreateWithName("test");

   test_sin = add_sincos_test(module, TRUE);
   test_cos = add_sincos_test(module, FALSE);

   if(LLVMVerifyModule(module, LLVMPrintMessageAction, &error)) {
      printf("LLVMVerifyModule: %s\n", error);
      LLVMDumpModule(module);
      abort();
   }
   LLVMDisposeMessage(error);

#if 0
   pass = LLVMCreatePassManager();
   LLVMAddTargetData(LLVMGetExecutionEngineTargetData(engine), pass);
   /* These are the passes currently listed in llvm-c/Transforms/Scalar.h,
    * but there are more on SVN. */
   LLVMAddConstantPropagationPass(pass);
   LLVMAddInstructionCombiningPass(pass);
   LLVMAddPromoteMemoryToRegisterPass(pass);
   LLVMAddGVNPass(pass);
   LLVMAddCFGSimplificationPass(pass);
   LLVMRunPassManager(pass, module);
#else
   (void)pass;
#endif

   sin_func = (test_sincos_t) pointer_to_func(LLVMGetPointerToGlobal(engine, test_sin));
   cos_func = (test_sincos_t) pointer_to_func(LLVMGetPointerToGlobal(engine, test_cos));

   memset(unpacked, 0, sizeof unpacked);


   // LLVMDumpModule(module);
   {
      v4sf src = {3.14159/4.0, -3.14159/4.0, 1.0, -1.0};
      printv("ref ",sin_ps(src));
      printv("llvm", sin_func(src));
      printv("ref ",cos_ps(src));
      printv("llvm",cos_func(src));
   }

   LLVMFreeMachineCodeForFunction(engine, test_sin);
   LLVMFreeMachineCodeForFunction(engine, test_cos);

   if(pass)
      LLVMDisposePassManager(pass);

   return success;
}

#else /* !PIPE_ARCH_SSE */

static boolean
test_sincos(unsigned verbose, FILE *fp)
{
   return TRUE;
}

#endif /* !PIPE_ARCH_SSE */


boolean
test_all(unsigned verbose, FILE *fp)
{
   boolean success = TRUE;

   test_sincos(verbose, fp);

   return success;
}


boolean
test_some(unsigned verbose, FILE *fp, unsigned long n)
{
   return test_all(verbose, fp);
}

boolean
test_single(unsigned verbose, FILE *fp)
{
   printf("no test_single()");
   return TRUE;
}
