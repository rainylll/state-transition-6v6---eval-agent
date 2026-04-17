#!/usr/bin/env bash

set -euo pipefail

arch="$(uname -s)"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$script_dir"
build_dir="$repo_root/build/encounter-runner"
tasks_source="$repo_root/agent_mvp/data_real/raw/simulation_tasks.jsonl"
tasks_local="$repo_root/simulation_tasks.jsonl"
episodes_local="$repo_root/episodes.jsonl"
rollouts_local="$repo_root/rollouts.jsonl"
episodes_out_dir="$repo_root/agent_mvp/data_real/raw"
episodes_out="$episodes_out_dir/episodes.jsonl"
rollouts_out="$episodes_out_dir/rollouts.jsonl"
replay_dir="$repo_root/agent_mvp/data_real/replays"
replay_count=10

configuration="Release"
skip_build=0
skip_run=0

print_usage() {
  cat <<'EOF'
Usage: ./build_runner.sh [--configuration Release|Debug] [--skip-build] [--skip-run]

Options:
  --configuration   Build configuration. Default: Release
  --skip-build      Reuse an existing EncounterBatchRunner binary
  --skip-run        Configure/build only, do not execute the runner
  -h, --help        Show this help message
EOF
}

write_stage() {
  printf '\n==== %s ====\n' "$1"
}

write_detail() {
  printf '%s\n' "$1"
}

while (($# > 0)); do
  case "$1" in
    --configuration)
      if (($# < 2)); then
        echo "Missing value for --configuration" >&2
        exit 1
      fi
      configuration="$2"
      shift 2
      ;;
    --skip-build)
      skip_build=1
      shift
      ;;
    --skip-run)
      skip_run=1
      shift
      ;;
    -h|--help)
      print_usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      print_usage >&2
      exit 1
      ;;
  esac
done

case "$configuration" in
  Release|Debug)
    ;;
  *)
    echo "Unsupported configuration: $configuration" >&2
    exit 1
    ;;
esac

runner_path_candidates=(
  "$build_dir/bin/$configuration/EncounterBatchRunner"
  "$build_dir/bin/EncounterBatchRunner"
)
runner_exe="${runner_path_candidates[0]}"

write_stage "Runner configuration"
write_detail "Repo root: $repo_root"
write_detail "Build dir: $build_dir"
write_detail "Configuration: $configuration"
write_detail "Host OS: $arch"
write_detail "SkipBuild: $skip_build"
write_detail "SkipRun: $skip_run"
write_detail "Replay dir: $replay_dir"
write_detail "Replay files per run: $replay_count"

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake not found in PATH. Please install CMake first." >&2
  exit 1
fi

if ((skip_build == 0)); then
  write_stage "Configure EncounterBatchRunner (CMake)"
  cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE="$configuration"

  write_stage "Build EncounterBatchRunner"
  cmake --build "$build_dir" --config "$configuration" --target EncounterBatchRunner
fi

for candidate in "${runner_path_candidates[@]}"; do
  if [[ -x "$candidate" ]]; then
    runner_exe="$candidate"
    break
  fi
done

if [[ ! -x "$runner_exe" ]]; then
  echo "Runner executable not found: $runner_exe" >&2
  exit 1
fi

write_detail "Runner path: $runner_exe"

if ((skip_run == 0)); then
  write_stage "Prepare simulation input"
  if [[ ! -f "$tasks_source" ]]; then
    echo "Input tasks not found: $tasks_source" >&2
    exit 1
  fi

  rm -f "$episodes_local" "$rollouts_local"
  cp "$tasks_source" "$tasks_local"
  mkdir -p "$replay_dir" "$episodes_out_dir"
  find "$replay_dir" -maxdepth 1 -type f -name '*.acmi' -delete

  task_count="$(wc -l < "$tasks_source" | tr -d '[:space:]')"
  write_detail "Task source: $tasks_source"
  write_detail "Task count detected: $task_count"
  write_detail "Local runner input: $tasks_local"
  write_detail "Episodes output: $episodes_out"
  write_detail "Rollouts output: $rollouts_out"
  write_detail "Replay output dir: $replay_dir"

  write_stage "Run EncounterBatchRunner"
  start_ts="$(date +%s)"
  (
    cd "$repo_root"
    "$runner_exe"
  )
  end_ts="$(date +%s)"
  write_detail "Runner finished in $((end_ts - start_ts))s"

  if [[ ! -f "$episodes_local" ]]; then
    echo "Runner finished but episodes.jsonl was not produced at $episodes_local" >&2
    exit 1
  fi
  if [[ ! -f "$rollouts_local" ]]; then
    echo "Runner finished but rollouts.jsonl was not produced at $rollouts_local" >&2
    exit 1
  fi

  write_stage "Archive episodes.jsonl"
  mv -f "$episodes_local" "$episodes_out"
  mv -f "$rollouts_local" "$rollouts_out"
  episode_count="$(wc -l < "$episodes_out" | tr -d '[:space:]')"
  rollout_count="$(wc -l < "$rollouts_out" | tr -d '[:space:]')"
  replay_file_count="$(find "$replay_dir" -maxdepth 1 -type f -name '*.acmi' | wc -l | tr -d '[:space:]')"
  write_detail "Episodes archived to: $episodes_out"
  write_detail "Episode line count: $episode_count"
  write_detail "Rollouts archived to: $rollouts_out"
  write_detail "Rollout line count: $rollout_count"
  write_detail "Replay file count: $replay_file_count"
else
  write_stage "Skip run"
  write_detail "Build-only mode completed. Runner was not executed."
fi

write_stage "Done"
