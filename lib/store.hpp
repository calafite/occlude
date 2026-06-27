#pragma once

#include "common.hpp"
#include "state.hpp"

struct ManifestStore {
  ManifestStore(FilePath manifestPath);
  [[nodiscard]] Manifest load() const;   
  void save(Manifest const& manifest) const;

  private:
    FilePath path;
};
