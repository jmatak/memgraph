#pragma once

#include "list.hpp"
#include "identifier.hpp"

namespace ast
{

struct ReturnList : public List<Identifier, ReturnList>
{
    using List::List;
};

struct Return : public AstNode<Return>
{
    Return(ReturnList* return_list)
        : return_list(return_list) {}

    ReturnList* return_list;
};

};
