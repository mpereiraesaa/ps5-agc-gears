CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror
BUILD := build/host

.PHONY: all test shaders bsp-bundle bsp-inspect native native-release \
	bsp-native-release bsp-noclip-native-release audit clean
all: test audit

$(BUILD):
	mkdir -p $@

define test_rule
$(BUILD)/$(1): $(2) | $(BUILD)
	$(CC) $(CFLAGS) $$^ $(3) -o $$@
endef

$(eval $(call test_rule,test_gears_mesh,tests/test_gears_mesh.c src/gears_mesh.c,-lm))
$(eval $(call test_rule,test_gears_scene,tests/test_gears_scene.c src/gears_scene.c,-lm))
$(eval $(call test_rule,test_gears_frame_tracker,tests/test_gears_frame_tracker.c src/gears_frame_tracker.c,))
$(eval $(call test_rule,test_gears_draw_compose,tests/test_gears_draw_compose.c src/gears_draw_compose.c,))
$(eval $(call test_rule,test_gears_animation,tests/test_gears_animation.c src/gears_animation.c src/gears_frame_tracker.c src/gears_scene.c,-lm))
$(eval $(call test_rule,test_gears_telemetry,tests/test_gears_telemetry.c src/gears_telemetry.c,))
$(eval $(call test_rule,test_gears_frame_runner,tests/test_gears_frame_runner.c src/gears_frame_runner.c src/gears_animation.c src/gears_frame_tracker.c src/gears_scene.c src/gears_telemetry.c,-lm))
$(eval $(call test_rule,test_gears_rt_clear,tests/test_gears_rt_clear.c src/gears_rt_clear.c,))
$(eval $(call test_rule,test_gears_renderer,tests/test_gears_renderer.c src/gears_renderer.c src/gears_rt_clear.c src/gears_draw_compose.c,))
$(eval $(call test_rule,test_ps5_surface,tests/test_ps5_surface.c src/ps5_surface.c,))
$(eval $(call test_rule,test_ps5_present,tests/test_ps5_present.c src/ps5_present.c,))
$(eval $(call test_rule,test_ps5_frame_completion,tests/test_ps5_frame_completion.c src/ps5_frame_completion.c,))
$(eval $(call test_rule,test_ps5_agc_abi,tests/test_ps5_agc_abi.c native/stubs/libSceAgc.c native/stubs/libSceAgcDriver.c,))
$(eval $(call test_rule,test_ps5_color_target,tests/test_ps5_color_target.c src/ps5_color_target.c,))
$(eval $(call test_rule,test_ps5_depth_target,tests/test_ps5_depth_target.c src/ps5_depth_target.c,))
$(eval $(call test_rule,test_ps5_pipeline,tests/test_ps5_pipeline.c src/ps5_pipeline.c,))
$(eval $(call test_rule,test_ps5_event_adapter,tests/test_ps5_event_adapter.c src/ps5_event_adapter.c src/ps5_frame_completion.c,))
$(eval $(call test_rule,test_ps5_gpu_span,tests/test_ps5_gpu_span.c src/ps5_gpu_span.c,))
$(eval $(call test_rule,test_ps5_submission,tests/test_ps5_submission.c src/ps5_submission.c src/ps5_present.c,))
$(eval $(call test_rule,test_ps5_direct_memory,tests/test_ps5_direct_memory.c src/ps5_direct_memory.c,))
$(eval $(call test_rule,test_ps5_platform_abi,tests/test_ps5_platform_abi.c,))
$(eval $(call test_rule,test_ps5log_host,tests/test_ps5log_host.c native/ps5log/ps5log.c,-Inative/ps5log))
$(eval $(call test_rule,test_ps5_shader_header,tests/test_ps5_shader_header.c src/ps5_shader_header.c,))
$(eval $(call test_rule,test_ps5_agc_writer,tests/test_ps5_agc_writer.c src/ps5_agc_writer.c src/ps5_gpu_span.c,))
$(eval $(call test_rule,test_ps5_agc_submit,tests/test_ps5_agc_submit.c src/ps5_agc_submit.c src/ps5_gpu_span.c,))
$(eval $(call test_rule,test_ps5_videoout,tests/test_ps5_videoout.c src/ps5_videoout.c src/ps5_surface.c,))
$(eval $(call test_rule,test_bsp_bundle,tests/test_bsp_bundle.c src/bsp_bundle.c,))
$(eval $(call test_rule,test_bsp_command_plan,tests/test_bsp_command_plan.c src/bsp_command_plan.c src/bsp_flat_draw.c,))
$(eval $(call test_rule,test_bsp_flat_draw,tests/test_bsp_flat_draw.c src/bsp_flat_draw.c,))
$(eval $(call test_rule,test_bsp_flat_scene,tests/test_bsp_flat_scene.c src/bsp_flat_scene.c,-lm))
$(eval $(call test_rule,test_bsp_noclip,tests/test_bsp_noclip.c src/bsp_noclip.c,-lm))
$(eval $(call test_rule,test_bsp_runtime_plan,tests/test_bsp_runtime_plan.c src/bsp_runtime_plan.c,))
$(eval $(call test_rule,test_ps5_bump_allocator,tests/test_ps5_bump_allocator.c src/ps5_bump_allocator.c,))
$(eval $(call test_rule,inspect_bsp_bundle,tools/inspect_bsp_bundle.c src/bsp_bundle.c,-Isrc))

TESTS := test_gears_mesh test_gears_scene test_gears_frame_tracker \
	test_gears_draw_compose test_gears_animation test_gears_telemetry \
	test_gears_frame_runner test_gears_rt_clear test_gears_renderer \
	test_ps5_surface test_ps5_present test_ps5_frame_completion \
	test_ps5_agc_abi test_ps5_color_target test_ps5_depth_target \
	test_ps5_pipeline test_ps5_event_adapter test_ps5_gpu_span \
	test_ps5_submission test_ps5_direct_memory test_ps5_platform_abi \
	test_ps5log_host test_ps5_shader_header test_ps5_agc_writer \
	test_ps5_agc_submit test_ps5_videoout test_bsp_bundle test_bsp_command_plan \
	test_bsp_flat_draw test_bsp_flat_scene test_bsp_noclip test_bsp_runtime_plan \
	test_ps5_bump_allocator

test: $(addprefix $(BUILD)/,$(TESTS))
	@set -e; for test in $^; do $$test; done
	python3 tests/test_shader_contract.py
	python3 tests/test_build_shader.py
	python3 tests/test_generate_agc_metadata.py
	python3 tests/test_generate_bsp_build_metadata.py
	python3 tests/test_native_contract.py
	python3 tests/test_bake_bsp.py
	python3 tests/test_validate_bsp_noclip_evidence.py
	rm -rf build tools/__pycache__ tests/__pycache__

bsp-bundle: $(BUILD)/inspect_bsp_bundle
	@test -n "$(BSP_INPUT)" || { echo 'BSP_INPUT is required' >&2; exit 2; }
	mkdir -p build/bsp
	python3 tools/bake_bsp.py "$(BSP_INPUT)" build/bsp/map.ps5bsp
	$(BUILD)/inspect_bsp_bundle build/bsp/map.ps5bsp

bsp-inspect: $(BUILD)/inspect_bsp_bundle
	@test -n "$(BSP_BUNDLE)" || { echo 'BSP_BUNDLE is required' >&2; exit 2; }
	$(BUILD)/inspect_bsp_bundle "$(BSP_BUNDLE)"

shaders:
	@test -n "$(AMDLLPC)" || { echo 'AMDLLPC is required' >&2; exit 2; }
	@test -n "$(LLVM_READELF)" || { echo 'LLVM_READELF is required' >&2; exit 2; }
	python3 tools/build_shader.py --amdllpc "$(AMDLLPC)" \
		--readelf "$(LLVM_READELF)" --output-dir build/shaders
	python3 tools/build_shader.py --pipe shaders/bsp_flat.pipe --name bsp_flat \
		--amdllpc "$(AMDLLPC)" --readelf "$(LLVM_READELF)" \
		--output-dir build/shaders
	python3 tools/generate_agc_metadata.py \
		--manifest build/shaders/gears_lit.manifest.json \
		--output build/generated/gears_shader_metadata.h
	python3 tools/generate_agc_metadata.py \
		--manifest build/shaders/bsp_flat.manifest.json \
		--output build/generated/bsp_flat_shader_metadata.h \
		--prefix BSP_FLAT --symbol-prefix ps5_bsp_flat

native:
	bash tools/build_native.sh

native-release:
	bash tools/build_native.sh

bsp-native-release: bsp-bundle
	BSP_BUNDLE="$(CURDIR)/build/bsp/map.ps5bsp" bash tools/build_native.sh

bsp-noclip-native-release: bsp-bundle
	BSP_BUNDLE="$(CURDIR)/build/bsp/map.ps5bsp" BSP_NOCLIP=1 \
		bash tools/build_native.sh

audit:
	python3 tools/audit_publication.py

clean:
	rm -rf build dist tools/__pycache__ tests/__pycache__
