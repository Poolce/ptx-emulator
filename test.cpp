void movInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    using ET = EnumTable<dataType, roundingMode>;

    static constexpr auto table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array{+[](movInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                          {
                              [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                                  self->ExecuteThread<ET::decode<Is, Ks>()...>(lid, wc);
                              }(std::index_sequence_for<dataType, roundingMode>{});
                          }...};
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_, rounding_);
    for (uint32_t lid = 0; lid < wc->GetWarpSize(); ++lid)
        table[idx](this, lid, wc);
}