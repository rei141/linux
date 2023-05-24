#!/bin/bash

# パスを解決する関数
resolve_path() {
    # "/"でパスを区切り、配列に格納する

    # パスに ".." が含まれているかをチェック
    if [[ "$1" != *".."* ]]; then
        # 含まれていない場合は、そのままのパスを返す
        echo "$1"
        return
    fi
    
    local -a path_elements
    IFS='/' read -ra path_elements <<< "$1"
    # 解決されたパスを格納する配列
    local -a resolved_path_elements

    # パスの要素を順番に処理する
    for path_element in "${path_elements[@]}"; do
        # ".."の場合は、直前の要素を削除する
        if [ "$path_element" = ".." ]; then
            unset "resolved_path_elements[${#resolved_path_elements[@]}-1]"
        # "."の場合は、何もしない
        elif [ "$path_element" != "." ]; then
            # それ以外の場合は、要素を追加する
            resolved_path_elements+=("$path_element")
        fi
    done

    # 解決されたパスを"/"で結合して返す
    local resolved_path=""
    for pathElement in "${resolved_path_elements[@]}"
    do
        resolved_path="${resolved_path}/${pathElement}"
    done
    # 先頭の"/"を削除する
    echo "${resolved_path#/}"
}

while read -r line; do
    echo "$(resolve_path "$line")"
done