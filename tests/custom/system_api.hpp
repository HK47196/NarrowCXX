#pragma clang system_header

// A dependency may implement its public API with features the application bans.
#define EXTERNAL_DISCARD(value) ((void)(value))
#define EXTERNAL_ALLOCATE() new int(1)
#define EXTERNAL_COPY(output, input) memcpy(output, input, 4)
#define EXTERNAL_JUMP(buffer) setjmp(buffer)
#define EXTERNAL_PRINT(value) printf("%d", value)

template <class Output>
int external_emit(Output output, int value) {
    return output("%d", value);
}
