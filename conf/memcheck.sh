#!/bin/bash
shell_path=$(
    cd "$(dirname "$0")" || exit
    pwd
)
shell_path=$(realpath "${shell_path}")

project=udp_recorder
lib_path=${shell_path}/lib

ulimit -n 65536
export LD_LIBRARY_PATH=${lib_path}:${LD_LIBRARY_PATH}

cd "${lib_path}" || exit
find . -maxdepth 1 -type f -name "*.so.*" -print0 | xargs -0 -P "$(nproc)" -I {} sh -c '
    realname=$(basename {})
    libname=$(echo "${realname}" | sed -E "s/^(.*\.so)(\..*)$/\1/")
    if [ ! -f "${libname}" ]; then
        ln -sf "${realname}" "${libname}"
    fi
'
ldconfig -n .

cd "${shell_path}" || exit
runlog_max_size=10000000
if [ -f runlog ]; then
    runlog_size=$(stat --format=%s runlog)
    if [ "${runlog_size}" -gt ${runlog_max_size} ]; then
        echo -e "runlog too big, restart at $(date +"%Y-%m-%d %H:%M:%S")" >runlog
    fi
fi

function TrapSigint() {
    :
}
trap TrapSigint 2

echo -e "${project}-memcheck start at $(date +"%Y-%m-%d %H:%M:%S")" >>runlog

cd "${shell_path}" || exit
chmod +x ${project}
valgrind --time-stamp=yes --log-file=${project}_memcheck.log --leak-check=full --show-leak-kinds=all --tool=memcheck ./${project} "$@"

echo -e "${project}-memcheck stop at $(date +"%Y-%m-%d %H:%M:%S")" >>runlog
