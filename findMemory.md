# `FindMemory` 해석

대상 함수: [`main.c`](main.c)의 `FindMemory`

## 역할

`FindMemory(AS, arg_size)`는 `AS->ptr`에 기록된 비트맵을 조사하여 `arg_size`만큼 연속으로 비어 있는 메모리 위치를 찾습니다. 적합한 위치를 찾지 못하면 `ExtendSpace`로 관리 공간을 4배 확장한 뒤 같은 요청을 재귀적으로 다시 처리합니다.

이 함수에서 비트맵은 다음처럼 해석됩니다.

- `all`: 해당 범위가 모두 사용 중인지 나타내는 비트맵
- `any`: 해당 범위의 일부가 사용 중인지 나타내는 비트맵
- `~all & ~any`: 해당 범위가 비어 있을 가능성이 있는 비트맵
- `toContiBit(mask, size)`: `size`개 이상의 연속된 빈 비트를 남김
- `ofLeftBit(mask)`: 남은 비트맵에서 가장 왼쪽의 후보 위치를 반환

## 단계별 흐름

1. 요청 크기와 현재 주소 공간 크기를 각각 레벨로 변환합니다.
2. 상위 레벨을 내려가며 `CallFindMaskPoint0X`로 일부라도 비어 있는 하위 범위를 찾습니다.
3. 후보 범위에서 `CallFindMaskContiPoint`를 호출하여 요청 크기만큼 연속된 빈 공간을 찾습니다.
4. 중간 레벨에 걸친 경우 64개 하위 항목을 훑어 후보 마스크를 만들고, 다시 연속 공간을 확인합니다.
5. `CallFindMaskContiPoint(AS, level, 0, arg_size)`로 전체 공간에서 요청 크기에 맞는 빈 위치를 찾습니다.
6. 전체 레벨에서 후보를 찾지 못하면 공간을 확장하고 `FindMemory`를 재호출합니다.
7. 확장 없이 후보를 찾았으면 해당 인덱스를 반환합니다.

## 반환값 선택

```c
INT return_index = (index_any == -1 ? point_all : index_any);
```

- `index_any`가 유효하면 더 낮은 레벨에서 찾은 후보를 우선합니다.
- `index_any`가 `-1`이면 전체 레벨 후보인 `point_all`을 사용합니다.
- `point_all == -1`이면 현재 계산한 반환값을 사용하지 않고 공간을 확장한 뒤 재시도합니다.

## 플로우 차트

```mermaid
flowchart TD
    A([FindMemory 시작<br/>AS, arg_size]) --> B[요청 크기 레벨 계산<br/>sizeToLevel = ofLevel(arg_size)<br/>level = ofLevel(AS->ptr_size)]
    B --> C[상위 레벨에서 일부 빈 범위 탐색<br/>CallFindMaskPoint0X 반복]
    C --> D[후보 범위에서 연속 빈 공간 탐색<br/>index_any_0 = CallFindMaskContiPoint]
    D --> E{중간 레벨인가?<br/>1 < i}
    E -- 예 --> F[64개 하위 항목 조사<br/>toContiBit로 후보 mask 생성]
    F --> G[중간 레벨 연속 공간 탐색<br/>index_any_64 계산]
    E -- 아니오 --> H[index_any = index_any_0]
    G --> I[index_any = index_any_64]
    H --> J[전체 레벨에서 연속 빈 공간 탐색<br/>point_all = CallFindMaskContiPoint]
    I --> J
    J --> K{point_all == -1?}
    K -- 아니오 --> L[index_any가 유효하면 index_any<br/>아니면 point_all 반환]
    K -- 예 --> M[ExtendSpace(AS)<br/>ptr_size를 4배 확장]
    M --> N[FindMemory 재귀 호출]
    N --> B
    L --> O([종료])
```

## 현재 코드에서 확인할 점

### `point_all`의 중간 계산이 덮어써짐

다음 반복문에서 `point_all`을 상위 레벨부터 계산하지만, 바로 다음 문장에서 결과를 다시 대입합니다.

```c
point_all = CallFindMaskContiPoint(AS, level, 0, arg_size);
```

따라서 현재 실행 경로에서는 그 앞의 `CallFindMaskPoint00` 반복 결과가 사용되지 않습니다. 의도한 계층적 탐색이라면 이 대입이 필요한지 확인해야 합니다.

### `space_size`와 `size`의 사용 여부

`space_size`와 `size`는 계산되지만, `space_size`는 함수 안에서 사용되지 않습니다. `size`는 중간 레벨의 64개 항목을 검사할 때만 사용됩니다.

### 실패값과 부호

`ofLeftBit`는 실패할 때 `-1`을 반환하지만 반환형과 주요 인덱스가 `uint64_t` 별칭인 `INT`입니다. 따라서 `-1` 비교가 의도대로 동작하는지, 그리고 유효한 인덱스와 실패값을 구분할 수 있는지 확인하는 것이 좋습니다.

### 경계 조건

`arg_size`가 0이거나 너무 큰 경우, 그리고 `ofLevel`에 전달되는 값이 0인 경우에 대한 방어가 없습니다. 호출자가 양수 크기와 지원 가능한 최대 크기만 전달한다는 전제가 필요합니다.
