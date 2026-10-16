struct Interface {
    virtual ~Interface() = default;
    virtual int evaluate() const = 0;
};
struct Implementation final : Interface {
    int evaluate() const override { return 1; }
};
struct Data { int value; };
struct DerivedData : Data {}; // expect: narrow-inheritance
struct Multiple : Interface, Data {}; // expect: narrow-no-complex-inheritance
struct Virtual : virtual Interface {}; // expect: narrow-no-complex-inheritance
struct Private : private Interface {}; // expect: narrow-inheritance
struct Protected : protected Interface {}; // expect: narrow-inheritance
struct Intermediate : Interface {};
struct DomainHierarchy : Intermediate {}; // expect: narrow-inheritance
