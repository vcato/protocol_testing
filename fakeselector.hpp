#include "selector.hpp"


class FakeSelector : public AbstractSelector {
  public:
    FakeSelector(const std::vector<FakeSelectable*> &fake_selectables_arg)
    : select_params_wrapper{select_params},
      fake_selectables(fake_selectables_arg)
    {
    }

    void addSelectable(FakeSelectable &arg)
    {
      fake_selectables.push_back(&arg);
    }

  private:
    FakeSelectParams select_params;
    BasicSelectParamsWrapper<FakeSelectParams> select_params_wrapper;
    std::vector<FakeSelectable *> fake_selectables;

    SelectParamsInterface &_selectParams() override
    {
      return select_params_wrapper;
    }

    int maxFileDescriptors() const
    {
      int result = 0;

      for (FakeSelectable *selectable_ptr : fake_selectables) {
        assert(selectable_ptr);

        if (selectable_ptr->nFileDescriptors() > result) {
          result = selectable_ptr->nFileDescriptors();
        }
      }

      return result;
    }

    void _setupSelect() override
    {
      select_params.setupSelect(maxFileDescriptors());
    }

    void _doSelect() override
    {
      for (FakeSelectable *selectable_ptr : fake_selectables) {
        assert(selectable_ptr);
        selectable_ptr->select(select_params);
      }
    }
};
