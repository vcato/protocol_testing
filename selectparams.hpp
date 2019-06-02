#ifndef SELECTPARAMS_HPP_
#define SELECTPARAMS_HPP_


struct PreSelectParamsInterface {
  virtual void setRead(int) = 0;
  virtual void setWrite(int) = 0;
};


struct PostSelectParamsInterface {
  virtual bool readIsSet(int) const = 0;
  virtual bool writeIsSet(int) const = 0;
};


struct SelectParamsInterface
: PreSelectParamsInterface, PostSelectParamsInterface
{
};


template <typename SelectParams>
struct BasicSelectParamsWrapper : SelectParamsInterface {
  SelectParams &select_params;

  BasicSelectParamsWrapper(SelectParams &select_params_arg)
  : select_params(select_params_arg)
  {
  }

  void setRead(int fd) override
  {
    select_params.setRead(fd);
  }

  void setWrite(int fd) override
  {
    select_params.setWrite(fd);
  }

  bool readIsSet(int fd) const override
  {
    return select_params.readIsSet(fd);
  }

  bool writeIsSet(int fd) const override
  {
    return select_params.writeIsSet(fd);
  }
};


#endif /* SELECTPARAMS_HPP_ */
