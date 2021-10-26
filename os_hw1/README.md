# OS HW1 - CPU 스케쥴러 구현
### 파일 구성
###### scheduler02.c : 최종 제출본
###### scheduler01.c : 초안


###  기존 문제점 
######  NPROC, NIOREQ의 값이 증가하면 실행속도 매우 느려짐
###  개선 사항
###### 1. 각 스케쥴러에서 fetch할 Process 선택 시 기존에는 PCB에서 찾았으나 연결리스트 내에서 탐색 하는 것으로 변경 -> 비교횟수 감소
###### 2. IO Done 메커니즘 대폭 수정 (과제 명세사항을 잘못 이해했음)

### 이하 코드 설명 일부
![1](https://user-images.githubusercontent.com/77635421/138823659-5e30a1f8-8ea5-4acb-9d2d-31526c7eeca9.jpg)
