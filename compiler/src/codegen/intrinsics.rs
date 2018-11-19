use codegen::{build_context_function, util, BuilderContext, TargetProperties};
use inkwell::attribute::AttrKind;
use inkwell::module::{Linkage, Module};
use inkwell::targets::TargetData;
use inkwell::types::{BasicType, VectorType};
use inkwell::values::FunctionValue;
use inkwell::{AddressSpace, FloatPredicate, IntPredicate};

pub fn memcpy(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.memcpy.p0i8.p0i8.i64", false, &|| {
        let context = module.get_context();
        (
            Linkage::ExternalLinkage,
            context.void_type().fn_type(
                &[
                    &context.i8_type().ptr_type(AddressSpace::Generic),
                    &context.i8_type().ptr_type(AddressSpace::Generic),
                    &context.i64_type(),
                    &context.i32_type(),
                    &context.bool_type(),
                ],
                false,
            ),
        )
    })
}

pub fn realloc(module: &Module, target: &TargetData) -> FunctionValue {
    util::get_or_create_func(module, "realloc", false, &|| {
        let ptr_type = module
            .get_context()
            .i8_type()
            .ptr_type(AddressSpace::Generic);
        (
            Linkage::ExternalLinkage,
            ptr_type.fn_type(
                &[
                    &ptr_type,
                    &target.int_ptr_type_in_context(&module.get_context()),
                ],
                false,
            ),
        )
    })
}

pub fn memset(module: &Module, target: &TargetData) -> FunctionValue {
    let target_ptr_type = target.int_ptr_type_in_context(&module.get_context());
    let intrinsic_name = format!("llvm.memset.p0i8.i{}", target_ptr_type.get_bit_width());
    util::get_or_create_func(module, &intrinsic_name, false, &|| {
        let ptr_type = module
            .get_context()
            .i8_type()
            .ptr_type(AddressSpace::Generic);
        (
            Linkage::ExternalLinkage,
            module.get_context().void_type().fn_type(
                &[
                    &ptr_type,
                    &module.get_context().i8_type(),
                    &target_ptr_type,
                    &module.get_context().i32_type(),
                    &module.get_context().bool_type(),
                ],
                false,
            ),
        )
    })
}

pub fn pow_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.pow.v2f64", false, &|| {
        let context = module.get_context();
        let v2f64_type = context.f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type, &v2f64_type], false),
        )
    })
}

pub fn pow_f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.pow.f64", false, &|| {
        let context = module.get_context();
        let f64_type = context.f64_type();
        (
            Linkage::ExternalLinkage,
            f64_type.fn_type(&[&f64_type, &f64_type], false),
        )
    })
}

pub fn exp_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.exp.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn exp2_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.exp2.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn log_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.log.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn log10_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.log10.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn log2_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.log2.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn cos_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.cos.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn cos_f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.cos.f64", false, &|| {
        let f64_type = module.get_context().f64_type();
        (
            Linkage::ExternalLinkage,
            f64_type.fn_type(&[&f64_type], false),
        )
    })
}

pub fn sin_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.sin.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn sin_f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.sin.f64", false, &|| {
        let f64_type = module.get_context().f64_type();
        (
            Linkage::ExternalLinkage,
            f64_type.fn_type(&[&f64_type], false),
        )
    })
}

pub fn sqrt_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.sqrt.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn ceil_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.ceil.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn floor_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.floor.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn fabs_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.fabs.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn minnum_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.minnum.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type, &v2f64_type], false),
        )
    })
}

pub fn minnum_f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.minnum.f64", false, &|| {
        let f64_type = module.get_context().f64_type();
        (
            Linkage::ExternalLinkage,
            f64_type.fn_type(&[&f64_type, &f64_type], false),
        )
    })
}

pub fn maxnum_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.maxnum.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type, &v2f64_type], false),
        )
    })
}

pub fn maxnum_f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.maxnum.f64", false, &|| {
        let f64_type = module.get_context().f64_type();
        (
            Linkage::ExternalLinkage,
            f64_type.fn_type(&[&f64_type, &f64_type], false),
        )
    })
}

pub fn ctlz_i64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.ctlz.i64", false, &|| {
        let i64_type = module.get_context().i64_type();
        (
            Linkage::ExternalLinkage,
            i64_type.fn_type(&[&i64_type, &module.get_context().bool_type()], false),
        )
    })
}

pub fn copysign_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "llvm.copysign.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::ExternalLinkage,
            v2f64_type.fn_type(&[&v2f64_type, &v2f64_type], false),
        )
    })
}

pub fn eucrem_v2i32(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "maxim.eucrem.v2i32", false, &|| {
        let v2i32_type = module.get_context().i32_type().vec_type(2);
        (
            Linkage::PrivateLinkage,
            v2i32_type.fn_type(&[&v2i32_type, &v2i32_type], false),
        )
    })
}

pub fn fract_v2f64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "maxim.fract.v2f64", false, &|| {
        let v2f64_type = module.get_context().f64_type().vec_type(2);
        (
            Linkage::PrivateLinkage,
            v2f64_type.fn_type(&[&v2f64_type], false),
        )
    })
}

pub fn next_power_i64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "maxim.nextpower.i64", false, &|| {
        let i64_type = module.get_context().i64_type();
        (
            Linkage::PrivateLinkage,
            i64_type.fn_type(&[&i64_type], false),
        )
    })
}

pub fn profile_timestamp_i64(module: &Module) -> FunctionValue {
    util::get_or_create_func(module, "profile_timestamp", true, &|| {
        let i64_type = module.get_context().i64_type();
        (Linkage::ExternalLinkage, i64_type.fn_type(&[], false))
    })
}

pub fn build_intrinsics(module: &Module, target: &TargetProperties) {
    //build_pow_v2f64(module, target);
    //build_pow_f64(module, target);
    /*build_exp_v2f64(module, target);
    build_log_v2f64(module, target);
    build_log10_v2f64(module, target);
    build_log2_v2f64(module, target);
    build_sin_v2f64(module, target);
    build_cos_v2f64(module, target);
    build_tan_v2f64(module, target);*/
    build_eucrem_v2i32(module, target);
    build_fract_v2f64(module, target);
    build_next_power_i64(module, target);
}

fn build_eucrem_v2i32(module: &Module, target: &TargetProperties) {
    build_context_function(
        module,
        eucrem_v2i32(module),
        target,
        &|ctx: BuilderContext| {
            let x_val = ctx.func.get_nth_param(0).unwrap().into_vector_value();
            let y_val = ctx.func.get_nth_param(1).unwrap().into_vector_value();

            // if we assume y is always positive (which we do here), this is equal to
            // x % y when x is positive, otherwise x % y + y
            let rem_val = ctx.b.build_int_signed_rem(x_val, y_val, "rem");
            let const_zero = VectorType::const_vector(&[
                &ctx.context.i32_type().const_int(0, false),
                &ctx.context.i32_type().const_int(0, false),
            ]);
            let lt_zero = ctx
                .b
                .build_int_compare(IntPredicate::SLT, x_val, const_zero, "");
            let shift_amt = ctx.b.build_int_nuw_mul(
                y_val,
                ctx.b.build_int_z_extend(lt_zero, y_val.get_type(), ""),
                "",
            );

            let result_val = ctx.b.build_int_nsw_add(rem_val, shift_amt, "");
            ctx.b.build_return(Some(&result_val));
        },
    );
}

fn build_fract_v2f64(module: &Module, target: &TargetProperties) {
    build_context_function(
        module,
        fract_v2f64(module),
        target,
        &|ctx: BuilderContext| {
            let floor_intrinsic = floor_v2f64(ctx.module);
            let x_val = ctx.func.get_nth_param(0).unwrap().into_vector_value();

            let x_floored = ctx
                .b
                .build_call(&floor_intrinsic, &[&x_val], "xfloored", false)
                .left()
                .unwrap()
                .into_vector_value();

            let fract_val = ctx.b.build_float_sub(x_val, x_floored, "xfract");

            ctx.b.build_return(Some(&fract_val));
        },
    )
}

fn build_next_power_i64(module: &Module, target: &TargetProperties) {
    build_context_function(
        module,
        next_power_i64(module),
        target,
        &|ctx: BuilderContext| {
            let ctlz_intrinsic = ctlz_i64(ctx.module);

            let zero_true_block = ctx.context.append_basic_block(&ctx.func, "zero.true");
            let zero_false_block = ctx.context.append_basic_block(&ctx.func, "zero.false");

            let in_val = ctx.func.get_nth_param(0).unwrap().into_int_value();

            // we need special behavior for 0 since this results in 64 << 64, undefined behavior in LLVM
            let is_zero = ctx.b.build_int_compare(
                IntPredicate::EQ,
                in_val,
                ctx.context.i64_type().const_int(0, false),
                "zero",
            );
            ctx.b
                .build_conditional_branch(&is_zero, &zero_true_block, &zero_false_block);

            ctx.b.position_at_end(&zero_true_block);
            ctx.b
                .build_return(Some(&ctx.context.i64_type().const_int(0, false)));

            ctx.b.position_at_end(&zero_false_block);
            let next_pow_val = ctx.b.build_left_shift(
                ctx.context.i64_type().const_int(1, false),
                ctx.b.build_int_nuw_sub(
                    ctx.context.i64_type().const_int(64, false),
                    ctx.b
                        .build_call(
                            &ctlz_intrinsic,
                            &[
                                &ctx.b.build_int_nuw_sub(
                                    in_val,
                                    ctx.context.i64_type().const_int(1, false),
                                    "",
                                ),
                                &ctx.context.bool_type().const_int(0, false),
                            ],
                            "",
                            false,
                        ).left()
                        .unwrap()
                        .into_int_value(),
                    "",
                ),
                "result",
            );
            ctx.b.build_return(Some(&next_pow_val));
        },
    );
}
