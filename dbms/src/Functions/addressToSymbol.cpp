#include <Common/SymbolIndex.h>
#include <Columns/ColumnString.h>
#include <Columns/ColumnsNumber.h>
#include <DataTypes/DataTypeString.h>
#include <Functions/IFunction.h>
#include <Functions/FunctionHelpers.h>
#include <Functions/FunctionFactory.h>
#include <Interpreters/Context.h>
#include <IO/WriteHelpers.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int ILLEGAL_COLUMN;
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
    extern const int NUMBER_OF_ARGUMENTS_DOESNT_MATCH;
    extern const int FUNCTION_NOT_ALLOWED;
}

class FunctionAddressToSymbol : public IFunction
{
public:
    static constexpr auto name = "addressToSymbol";
    static FunctionPtr create(const Context & context)
    {
        if (!context.getSettingsRef().allow_introspection_functions)
            throw Exception("Introspection functions are disabled, because setting 'allow_introspection_functions' is set to 0", ErrorCodes::FUNCTION_NOT_ALLOWED);
        return std::make_shared<FunctionAddressToSymbol>();
    }

    String getName() const override
    {
        return name;
    }

    size_t getNumberOfArguments() const override
    {
        return 1;
    }

    DataTypePtr getReturnTypeImpl(const ColumnsWithTypeAndName & arguments) const override
    {
        if (arguments.size() != 1)
            throw Exception("Function " + getName() + " needs exactly one argument; passed "
                + toString(arguments.size()) + ".", ErrorCodes::NUMBER_OF_ARGUMENTS_DOESNT_MATCH);

        const auto & type = arguments[0].type;

        if (!WhichDataType(type.get()).isUInt64())
            throw Exception("The only argument for function " + getName() + " must be UInt64. Found "
                + type->getName() + " instead.", ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT);

        return std::make_shared<DataTypeString>();
    }

    bool useDefaultImplementationForConstants() const override
    {
        return true;
    }

    void executeImpl(Block & block, const ColumnNumbers & arguments, size_t result, size_t input_rows_count) override
    {
        const SymbolIndex & symbol_index = SymbolIndex::instance();

        const ColumnPtr & column = block.getByPosition(arguments[0]).column;
        const ColumnUInt64 * column_concrete = checkAndGetColumn<ColumnUInt64>(column.get());

        if (!column_concrete)
            throw Exception("Illegal column " + column->getName() + " of argument of function " + getName(), ErrorCodes::ILLEGAL_COLUMN);

        const typename ColumnVector<UInt64>::Container & data = column_concrete->getData();
        auto result_column = ColumnString::create();

        for (size_t i = 0; i < input_rows_count; ++i)
        {
            if (const auto * symbol = symbol_index.findSymbol(reinterpret_cast<const void *>(data[i])))
                result_column->insertDataWithTerminatingZero(symbol->name, strlen(symbol->name) + 1);
            else
                result_column->insertDefault();
        }

        block.getByPosition(result).column = std::move(result_column);
    }
};

void registerFunctionAddressToSymbol(FunctionFactory & factory)
{
    factory.registerFunction<FunctionAddressToSymbol>();
}

}
