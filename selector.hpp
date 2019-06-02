#ifndef SELECTOR_HPP_
#define SELECTOR_HPP_


#include <cassert>
#include "selectparams.hpp"


struct AbstractSelector {
  public:
    void beginSelect()
    {
      assert(!in_pre_select);
      assert(!in_post_select);
      _setupSelect();
      in_pre_select = true;
    }

    PreSelectParamsInterface& preSelectParams()
    {
      assert(in_pre_select);
      return _selectParams();
    }

    void callSelect()
    {
      assert(in_pre_select);
      assert(!in_post_select);
      _doSelect();
      in_pre_select = false;
      in_post_select = true;
    }

    PostSelectParamsInterface &postSelectParams()
    {
      assert(in_post_select);
      return _selectParams();
    }

    void endSelect()
    {
      assert(!in_pre_select);
      assert(in_post_select);
      in_post_select = false;
    }

  private:
    virtual SelectParamsInterface &_selectParams() = 0;
    virtual void _setupSelect() = 0;
    virtual void _doSelect() = 0;

    bool in_pre_select = false;
    bool in_post_select = false;
};


#endif /* SELECTOR_HPP_ */
