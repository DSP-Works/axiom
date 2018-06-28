use codegen::{
    block, build_context_function, data_analyzer, util, values, BuilderContext, LifecycleFunc,
    TargetProperties,
};
use inkwell::builder::Builder;
use inkwell::module::{Linkage, Module};
use inkwell::values::{FunctionValue, PointerValue};
use inkwell::{AddressSpace, IntPredicate};
use mir::{MIRContext, Node, NodeData, Surface};

fn get_lifecycle_func(
    module: &Module,
    mir: &MIRContext,
    surface: &Surface,
    target: &TargetProperties,
    lifecycle: LifecycleFunc,
) -> FunctionValue {
    let func_name = format!(
        "maxim.surface.{}.{}.{}",
        surface.id.id, surface.id.debug_name, lifecycle
    );
    util::get_or_create_func(module, &func_name, &|| {
        let context = module.get_context();
        let layout = data_analyzer::build_surface_layout(&context, mir, surface, target);
        (
            Linkage::ExternalLinkage,
            context.void_type().fn_type(
                &[
                    &layout.scratch_struct.ptr_type(AddressSpace::Generic),
                    &layout.pointer_struct.ptr_type(AddressSpace::Generic),
                ],
                false,
            ),
        )
    })
}

fn build_node_call(
    ctx: &mut BuilderContext,
    mir: &MIRContext,
    node: &Node,
    target: &TargetProperties,
    lifecycle: LifecycleFunc,
    scratch_ptr: PointerValue,
    pointers_ptr: PointerValue,
) {
    match &node.data {
        NodeData::Custom(block_id) => {
            let block = mir.block(&block_id).unwrap();
            let data_ptr = unsafe { ctx.b.build_struct_gep(&scratch_ptr, 0, "") };
            let ui_ptr = if target.include_ui {
                Some(unsafe { ctx.b.build_struct_gep(&scratch_ptr, 1, "") })
            } else {
                None
            };

            block::build_lifecycle_call(
                ctx.module,
                ctx.b,
                block,
                target,
                lifecycle,
                data_ptr,
                pointers_ptr,
                ui_ptr,
            );
        }
        NodeData::Group(surface_id) => {
            let surface = mir.surface(&surface_id).unwrap();
            build_lifecycle_call(
                ctx.module,
                ctx.b,
                mir,
                surface,
                target,
                lifecycle,
                scratch_ptr,
                pointers_ptr,
            );
        }
        NodeData::ExtractGroup {
            surface: surface_id,
            source_sockets,
            dest_sockets,
        } => {
            let surface = mir.surface(&surface_id).unwrap();

            // if this is the update lifecycle function and there are source groups, generate a
            // bitmap of which indices are valid
            let valid_bitmap = if lifecycle == LifecycleFunc::Update && !source_sockets.is_empty() {
                let first_array = values::ArrayValue::new(
                    ctx.b
                        .build_load(
                            &unsafe {
                                ctx.b
                                    .build_struct_gep(&pointers_ptr, source_sockets[0] as u32, "")
                            },
                            "",
                        )
                        .into_pointer_value(),
                );
                let first_bitmap = first_array.get_bitmap(ctx.b);

                Some(
                    source_sockets
                        .iter()
                        .skip(1)
                        .fold(first_bitmap, |acc, socket| {
                            let nth_array = values::ArrayValue::new(
                                ctx.b
                                    .build_load(
                                        &unsafe {
                                            ctx.b.build_struct_gep(
                                                &pointers_ptr,
                                                *socket as u32,
                                                "",
                                            )
                                        },
                                        "",
                                    )
                                    .into_pointer_value(),
                            );
                            let nth_bitmap = nth_array.get_bitmap(ctx.b);
                            ctx.b.build_or(acc, nth_bitmap, "")
                        }),
                )
            } else {
                None
            };

            // build a for loop to iterate over each instance
            let index_ptr = ctx.allocb
                .build_alloca(&ctx.context.i8_type(), "voiceindex.ptr");

            let check_block = ctx.context.append_basic_block(&ctx.func, "voice.check");
            let check_active_block = ctx.context
                .append_basic_block(&ctx.func, "voice.checkactive");
            let run_block = ctx.context.append_basic_block(&ctx.func, "voice.run");
            let end_block = ctx.context.append_basic_block(&ctx.func, "voice.end");

            ctx.b.build_unconditional_branch(&check_block);
            ctx.b.position_at_end(&check_block);

            let current_index = ctx.b.build_load(&index_ptr, "voiceindex").into_int_value();
            let iter_limit = ctx.context
                .i8_type()
                .const_int(values::ARRAY_CAPACITY as u64, false);
            let can_continue_loop = ctx.b.build_int_compare(
                IntPredicate::ULT,
                current_index,
                iter_limit,
                "cancontinue",
            );

            ctx.b
                .build_conditional_branch(&can_continue_loop, &check_active_block, &end_block);
            ctx.b.position_at_end(&check_active_block);

            // increment the stored value
            let next_index = ctx.b.build_int_add(
                current_index,
                ctx.context.i8_type().const_int(1, false),
                "nextindex",
            );
            ctx.b.build_store(&index_ptr, &next_index);

            if let Some(active_bitmap) = valid_bitmap {
                // check if this iteration is active according to the bitmap
                let bitmask = ctx.b.build_left_shift(
                    ctx.context.i8_type().const_int(1, false),
                    current_index,
                    "bitmask",
                );
                let active_bits = ctx.b.build_and(active_bitmap, bitmask, "activebits");
                let active_bit = ctx.b.build_int_cast(
                    ctx.b
                        .build_right_shift(active_bits, current_index, false, "activebit"),
                    ctx.context.bool_type(),
                    "",
                );

                ctx.b
                    .build_conditional_branch(&active_bit, &run_block, &check_block);
            } else {
                ctx.b.build_unconditional_branch(&run_block);
            }

            ctx.b.position_at_end(&run_block);

            let voice_scratch_ptr = unsafe {
                ctx.b
                    .build_in_bounds_gep(&scratch_ptr, &[current_index], "scratchptr")
            };
            let voice_pointers_ptr = unsafe {
                ctx.b
                    .build_in_bounds_gep(&pointers_ptr, &[current_index], "pointersptr")
            };

            build_lifecycle_call(
                ctx.module,
                ctx.b,
                mir,
                surface,
                target,
                lifecycle,
                voice_scratch_ptr,
                voice_pointers_ptr,
            );

            ctx.b.build_unconditional_branch(&check_block);
            ctx.b.position_at_end(&end_block);

            // set the bitmaps of all output arrays to be this input
            if lifecycle == LifecycleFunc::Update {
                let active_bitmap = if let Some(active_bitmap) = valid_bitmap {
                    active_bitmap
                } else {
                    ctx.b
                        .build_not(&ctx.context.i8_type().const_int(0, false), "")
                };

                for dest_socket in dest_sockets {
                    let dest_array = values::ArrayValue::new(
                        ctx.b
                            .build_load(
                                &unsafe {
                                    ctx.b
                                        .build_struct_gep(&pointers_ptr, *dest_socket as u32, "")
                                },
                                "",
                            )
                            .into_pointer_value(),
                    );
                    dest_array.set_bitmap(ctx.b, &active_bitmap);
                }
            }
        }
    }
}

pub fn build_lifecycle_func(
    module: &Module,
    mir: &MIRContext,
    surface: &Surface,
    target: &TargetProperties,
    lifecycle: LifecycleFunc,
) {
    let func = get_lifecycle_func(module, mir, surface, target, lifecycle);
    build_context_function(module, func, target, &|mut ctx: BuilderContext| {
        let layout = data_analyzer::build_surface_layout(ctx.context, mir, surface, target);
        let scratch_ptr = ctx.func.get_nth_param(0).unwrap().into_pointer_value();
        let pointers_ptr = ctx.func.get_nth_param(1).unwrap().into_pointer_value();

        for (node_index, node) in surface.nodes.iter().enumerate() {
            let layout_ptr_index = layout.node_ptr_index(node_index);
            let node_scratch_ptr = unsafe {
                ctx.b.build_struct_gep(
                    &scratch_ptr,
                    layout.node_scratch_index(node_index) as u32,
                    "",
                )
            };
            let node_pointers_ptr = unsafe {
                ctx.b
                    .build_struct_gep(&pointers_ptr, layout_ptr_index as u32, "")
            };

            build_node_call(
                &mut ctx,
                mir,
                node,
                target,
                lifecycle,
                node_scratch_ptr,
                node_pointers_ptr,
            );
        }

        ctx.b.build_return(None);
    })
}

pub fn build_funcs(
    module: &Module,
    mir: &MIRContext,
    surface: &Surface,
    target: &TargetProperties,
) {
    build_lifecycle_func(module, mir, surface, target, LifecycleFunc::Construct);
    build_lifecycle_func(module, mir, surface, target, LifecycleFunc::Update);
    build_lifecycle_func(module, mir, surface, target, LifecycleFunc::Destruct);
}

pub fn build_lifecycle_call(
    module: &Module,
    builder: &mut Builder,
    mir: &MIRContext,
    surface: &Surface,
    target: &TargetProperties,
    lifecycle: LifecycleFunc,
    scratch_ptr: PointerValue,
    pointer_ptr: PointerValue,
) {
    let func = get_lifecycle_func(module, mir, surface, target, lifecycle);
    builder.build_call(&func, &[&scratch_ptr, &pointer_ptr], "", false);
}
